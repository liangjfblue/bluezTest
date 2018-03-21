#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

//需要编译bluez库，把头文件抠出来，并把.so库移植到板上
#include "bluetooth.h" 	//BTPROTO_HCI
#include "hci.h"          //struct hci_dev_info
#include "hci_lib.h"     //hci_devid()
#include "l2cap.h"      //l2cap
#include "hidp.h"       //hidp
#include "rfcomm.h"		//rfcomm协议

#define NULL ((void *) 0)

//保存bt的mac地址，在下次开机和异常断开时读取上次连接的bt，自动重连
#define BT_MAC_FILE "/tmp/bt_mac"

unsigned int epoll_fd_num;

pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;

int BTDevInit()
{
	int ctl = -1;
	char cmd[100] = {0};
	struct hci_dev_list_req *dl;//本地蓝牙设备表
	struct hci_dev_info di; // 其中某个蓝牙设备的信息，我们测试通常只插一个
	struct hci_dev_req *dr;
	static bdaddr_t locate_bdaddr;
	char locate_bt_mac[17] = {0};;
	if (!(dl = malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(unsigned short)))) {
		perror("Can't allocate memory");
		return -1;
	}
	dl->dev_num = HCI_MAX_DEV;
	dr = dl->dev_req;
	// 1.打开一个 HCI socket
	if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) {
		perror("Can't open HCI socket.");
		return -1;
	}
	// 2. 使用CIGETDEVLIST,得到所有dongle的Device ID。存放在dl中
	if (ioctl(ctl, HCIGETDEVLIST, (void *) dl) < 0) {
		perror("Can't get device list");
		return -1;
	}
	printf("Get hci dev num:%d\n",dl->dev_num);
	// 3.使用CIGETDEVINFO，得到对应Device ID的Dongle信息，调试一个
	//di.dev_id = (dr+i)->dev_id;
	di.dev_id = dr->dev_id;
	ioctl(ctl, HCIGETDEVINFO, (void *) &di);
	close(ctl);
	if(strlen(di.name) < 3) {
		printf("Can't get hci device.\n");
		return -1;
	}

	// 4.打开蓝牙-保留（一般不在代码打开，而是通过启动脚本，【/etc/init.d/bluetooth start】命令方式打开）
	/*if(ioctl(ctl, HCIDEVUP, di.dev_id) < 0) {
		perror("Can't enable ble device");
		return -1;
	}*/

	memcpy(&locate_bdaddr, &di.bdaddr, sizeof(bdaddr_t));
	ba2str(&di.bdaddr, locate_bt_mac);
	printf("Locate bt hci info:\n dev_id: %d\n name: %s\n mac: %s\n", di.dev_id, di.name, locate_bt_mac);
	sprintf(cmd, "echo %s > "BT_MAC_FILE" \n", locate_bt_mac);
	system(cmd);
	system("sync \n");
	return 0;
}

int BTInitSocket(void)
{
	int socket_fd = -1,ret = -1;
	struct sockaddr_rc loc_addr = {0};
	if(0 !=BTDevInit()) {
		printf("<%s,%d>liangjf: BT hardware init err[%s]\n", __func__,__LINE__, strerror(errno));
		return -1;
	}

	printf("<%s,%d>liangjf: Creating socket .\n", __func__,__LINE__);
	socket_fd =socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if(socket_fd<0) {
		printf("<%s,%d>liangjf: Create socket error[%s]\n", __func__,__LINE__, strerror(errno));
		return 1;
	}

	//设置socket属性，重用port端口。解决ctrl-c结束后再次启动端口复用问题
	int on=1;
	printf("<%s,%d>liangjf: set socketopt...\n", __func__,__LINE__);
	if(setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on))<0) {
		printf("<%s,%d>liangjf: set socketopt failed[%s]\n", __func__,__LINE__, strerror(errno));
		return -1;
	}

	printf("<%s,%d>liangjf: socket bind...\n", __func__,__LINE__);
	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = *BDADDR_ANY;
	loc_addr.rc_channel = 1;
	ret = bind(socket_fd,(struct sockaddr *)&loc_addr, sizeof(loc_addr));
	if(ret<0) {
		printf("<%s,%d>liangjf: bind socket error:ret=%d[%s]\n", __func__,__LINE__, ret, strerror(errno));
		return -1;
	}

	printf("<%s,%d>liangjf: listen... \n", __func__,__LINE__);
	ret=listen(socket_fd, 5);
	if(ret<0) {
		printf("<%s,%d>liangjf: listen error[%s]\n", __func__,__LINE__, strerror(errno));
		return -1;
	}
	//暂不设置超时，通过select设置
	printf("<%s,%d>liangjf: set socket opt .\n", __func__,__LINE__);
	struct timeval timeout = {0,10000};
	if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval)) != 0) {
		printf("<%s,%d>liangjf: set socket opt failed[%s]\n", __func__,__LINE__, strerror(errno));
		return -1;
	}

	printf("<%s,%d>liangjf: return socket_fd[%d]...\n", __func__,__LINE__, socket_fd);
	return socket_fd;
}

int setnonBlocking(int fd)
{
	int old_opt = fcntl(fd, F_GETFD);
	int new_opt = old_opt | O_NONBLOCK;
	fcntl(fd, F_SETFD, new_opt);

	return old_opt;
}

/**
 * 在每次调用epoll_ctrl 添加一个fd，自增1.
 * 目的是通过一个events数组管理全部fd
 */
static void AddEpollfdNum()
{
	pthread_mutex_lock(&fd_mutex);
	epoll_fd_num++;
	pthread_mutex_unlock(&fd_mutex);
	printf("<%s,%d>liangjf: now add one fd to epoll set\n", __func__,__LINE__);
}

int GetEpollNowfdNum(char *str)
{
	int ret_fd_num = 0;
	pthread_mutex_lock(&fd_mutex);
	ret_fd_num = epoll_fd_num;
	pthread_mutex_unlock(&fd_mutex);
	printf("<%s,%d>liangjf: %s,now epoll fd num is [%d]\n", __func__,__LINE__, str, ret_fd_num);
	return ret_fd_num;
}

/**
 * 添加fd到epoll监听
 */
int epollAddfd(struct epoll_event *event, int epollfd, int fd, int enable_et)
{
	int ret = -1;
	//获得现有epoll的fd的个数
	GetEpollNowfdNum("before add");

	event->data.fd = fd;
	event->events = EPOLLIN | EPOLLOUT;
	if(enable_et) {
		event->events |= EPOLLET;
	}
	ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, event);
	if ( ret < 0 ) {
		printf("<%s,%d>liangjf: epoll_ctl error[%s]\n", __func__,__LINE__, strerror(errno));
		return 1;
	}
	//epoll的fd的个数+1
	AddEpollfdNum();
	//获得现有epoll的fd的个数
	GetEpollNowfdNum("after add");
	return 0;
}

/**
 * 修改epoll集合中的fd
 */
int epollModfd(struct epoll_event *event, int epollfd, int fd, int enable_et)
{
	int ret = -1;
	//获得现有epoll的fd的个数
	GetEpollNowfdNum("before modify");
	event->data.fd = fd;
	event->events = EPOLLIN | EPOLLOUT;
	if(enable_et) {
		event->events |= EPOLLET;
	}
	ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, event);
	if ( ret < 0 ) {
		printf("<%s,%d>liangjf: epoll_ctl error[%s]\n", __func__,__LINE__, strerror(errno));
		return 1;
	}

	//获得现有epoll的fd的个数
	GetEpollNowfdNum("after modify");
	return 0;
}

/**
 * 删除epoll集合中的fd
 */
int epollDelfd(struct epoll_event *event, int epollfd, int fd, int enable_et)
{
	int ret = -1;
	//获得现有epoll的fd的个数
	GetEpollNowfdNum("before delete");
	ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
	if ( ret < 0 ) {
		printf("<%s,%d>liangjf: epoll_ctl error[%s]\n", __func__,__LINE__, strerror(errno));
	}

	pthread_mutex_lock(&fd_mutex);
	epoll_fd_num--;
	pthread_mutex_unlock(&fd_mutex);
	GetEpollNowfdNum("after delete");

	return 0;
}

/**
 * 删除epoll集合中的fd
 */
int RecvBT(int sockfd, char *r_buf, int r_len)
{
	int ret = recv(sockfd, r_buf, r_len, 0);
	if(ret < 0) {
		printf("<%s,%d>liangjf: recv error[%s]\n", __func__,__LINE__, strerror(errno));
		return -1;
	} else if(ret == 0) {
		printf("<%s,%d>liangjf: recv over[%s]\n", __func__,__LINE__, strerror(errno));
		return -2;
	}
	else {
		printf("<%s,%d>liangjf: recv data %d-%s.\n", __func__,__LINE__, ret, r_buf);
		return ret;
	}
}


int SendBT(int sockfd, char *w_buf, int w_len)
{
	int ret = send(sockfd, w_buf, w_len, 0);
	if(ret < 0) {
		printf("<%s,%d>liangjf: write error[%s]\n", __func__,__LINE__, strerror(errno));
		return -1;
	} else if(ret < w_len) {
		printf("<%s,%d>liangjf: write less data[%s]\n", __func__,__LINE__, strerror(errno));
		return -2;
	}
	else if(ret == w_len){
		printf("<%s,%d>liangjf: write data ok %d-%s.\n", __func__,__LINE__, ret, w_buf);
		return ret;
	}
	return -1;
}

/**
 * 真正处理非主socket fd的fd响应
 */
void do_use_fd(int epollfd, struct epoll_event *event)
{
	int ret = 0;
	char r_buf[1024];
	memset(r_buf, 0, sizeof(r_buf));

	if(event->events & EPOLLIN){	//异步通知：连接后有数据到来-读
		printf("<%s,%d>liangjf: has data read[EPOLLIN].\n", __func__,__LINE__);
		ret = RecvBT(event->data.fd, r_buf, sizeof(r_buf));

		//把读到的内容写文件

		//读文件内容通过蓝牙回传给客户端
		if(ret > 0) {
			printf("<%s,%d>liangjf: write data[EPOLLOUT]\n", __func__,__LINE__);
			SendBT(event->data.fd, r_buf, ret);
		}

		//重新修改epoll的里的监听fd，异步处理精髓
		epollModfd(event, epollfd, event->data.fd, 1);
	}
	else  if(event->events & EPOLLERR) {
		close(event->data.fd);
		printf("<%s,%d>liangjf: ble connect error[EPOLLERR]\n", __func__,__LINE__);
		epollDelfd(event, epollfd, event->data.fd, 1);
	}
	else if(event->events & EPOLLHUP) {
		close(event->data.fd);
		printf("<%s,%d>liangjf: ble hang up[EPOLLHUP]\n", __func__,__LINE__);
		epollDelfd(event, epollfd, event->data.fd, 1);
	}
}

/**
 *epoll监听fd
 */
int EpollBTorListends(struct epoll_event *event, struct epoll_event *events, int epollfd, int ret_fd_num, int listenfd)
{
	int i = 0, ret = -1;
	for(i = 0; i < ret_fd_num; i++) {
		if(events[i].data.fd == listenfd) { //异步通知：有蓝牙连接
			struct sockaddr_in clientaddr;
			int len = sizeof(clientaddr);
			memset(&clientaddr, 0, sizeof(clientaddr));

			//listenfd套接字准备好，客户端开始监听连接
			int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, (socklen_t *)&len);
			if (connfd < 0) {
				printf("<%s,%d>liangjf: accept socket error[%s]\n", __func__,__LINE__, strerror(errno));
				return -1;
			} else {
				printf("<%s,%d>liangjf: accept socket ok[%s]\n", __func__,__LINE__, inet_ntoa(clientaddr.sin_addr));
			}

			ret = epollAddfd(event, epollfd, connfd, 1);	//增加connfd被到epollfd监听集合中
			if(ret == 0) {
				printf("<%s,%d>liangjf: epoll Add fd ok\n", __func__,__LINE__);
			}
		} else {
			do_use_fd(epollfd, &events[i]);
		}
	}
	return 0;
}
