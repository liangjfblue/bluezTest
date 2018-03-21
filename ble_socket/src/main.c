/*
 * main.c
 *
 *  Created on: Mar 19, 2018
 *      Author: 梁杰帆-liangjf
 */
#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include<pthread.h>

#include "rfcomm.h"		//rfcomm协议
#include "ble.h"

#define NULL 	((void *)0)
#define MAX_EVENT_NUM	5

extern pthread_mutex_t fd_mutex;

int main(int argc, char *argv[])
{
	int listenfd = -1, cmd_ret = -1;
	char cmd_buf[1024] = {0};
	int epollfd = -1, ret = -1;
	int ret_fd_num = 0;
	struct epoll_event event, mevents[MAX_EVENT_NUM];

	//0.restart the bluetooth device first
	//ps打印进程，看是否有bluetooth，若没有就重启蓝牙
	FILE *fp = popen("ps|grep \"bluetooth\"|awk '{print $4}'", "r");
	if ( !fp ) {
		printf("<%s,%d>liangjf: popen error[%s]\n", __func__,__LINE__, strerror(errno));
		return 1;
	}
	cmd_ret = fread(cmd_buf, 1024, 1, fp);
	if ( cmd_ret < 0 ) {
		printf("<%s,%d>liangjf: fread cmd_buf error[%s]\n", __func__,__LINE__, strerror(errno));
		return 1;
	}
	if( strstr(cmd_buf, "bluetooth") == NULL) {
		//没有蓝牙进程就重启蓝牙
		system("/etc/init.d/bluetooth restart");
		printf("<%s,%d>liangjf: no bluetooth service, now has retart\n", __func__,__LINE__);
	}
	printf("<%s,%d>liangjf: bluetooth service has start\n", __func__,__LINE__);

	// 1. 初始化蓝牙设备/创建socket/绑定/监听
	listenfd = BTInitSocket();
	if ( listenfd < 0 ) {
		printf("<%s,%d>liangjf: BTInitSocket error[%s]\n", __func__,__LINE__, strerror(errno));
		return 1;
	}

	//2.创建一个epoll监听描述符，监听设置的文件描述符
	epollfd = epoll_create(10);
	if ( epollfd < 0 ) {
		printf("<%s,%d>liangjf: epoll_create error[%s]\n", __func__,__LINE__, strerror(errno));
		return 1;
	}

	//3.把socketfd加入epoll监听,et模式
	epollAddfd(&event, epollfd, listenfd, 1);

	printf("bluetooth all init ok, waiting to accept and read/write\n");
	while(1) {
		//4.异步等待blefd的响应
		ret_fd_num = epoll_wait(epollfd, mevents, 5, 0); //非阻塞 马上返回
		if ( ret_fd_num < 0 ) {
			printf("<%s,%d>liangjf: epoll_wait error[%s]\n", __func__,__LINE__, strerror(errno));
			return 1;
		}

		//5.选出异步通知的fd
		ret = EpollBTorListends(&event, mevents, epollfd, ret_fd_num, listenfd);
		if ( ret < 0 ) {
			printf("<%s,%d>liangjf: EpollBTorListends error[%s]\n", __func__,__LINE__, strerror(errno));
			return 1;
		}
	}

	//6.释放资源
	close(epollfd);
	pclose(fp);
	pthread_mutex_destroy(&fd_mutex);

	return 0;
}
