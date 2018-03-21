/*
 * ble.h
 *
 *  Created on: Mar 19, 2018
 *      Author: liangjf
 */

#ifndef INCLUDE_BLE_H_
#define INCLUDE_BLE_H_

extern unsigned int epoll_fd_num;
extern pthread_mutex_t fd_mutex;

int BTDevInit();
int GetEpollNowfdNum(char *str);
int BTInitSocket(void);
int epollAddfd(struct epoll_event *event, int epollfd, int fd, int enable_et);
int epollModfd(struct epoll_event *event, int epollfd, int fd, int enable_et);
int RecvBT(int sockfd, char *r_buf, int r_len);
int SendBT(int sockfd, char *w_buf, int w_len);
void do_use_fd(struct epoll_event *event, int epollfd, int fd);
int EpollBTorListends(struct epoll_event *event, struct epoll_event *events, int epollfd, int ret_fd_num, int listenfd);


#endif /* INCLUDE_BLE_H_ */
