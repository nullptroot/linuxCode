#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>
/*此程序就是设置一下信号处理函数 当有信号触发时通知主程序*/
#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

/*将文件设置为非阻塞的*/
int setnonblocking(int fd)
{
	int old_option = fcntl(fd,F_GETFL);/*获得fd的选项属性*/
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}
/*将fd上的EPOLLIN和EPOLLET事件注册到epollfd指示的epoll内核事件
 * 中，参数oneshot指定是否注册fd上的EPOLLONESHOT事件*/
void addfd(int epollfd,int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}
/*信号处理函数*/
void sig_handler(int sig)
{
	/*保留原来的errno，在函数最后回复，以保证函数的可重入性*/
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1],(char *) &msg,1,0);/*将信号值写入管道，以通知主循环*/
	errno = save_errno;
}
/*设置信号的处理函数*/
void addsig(int sig)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler = sig_handler;
	sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);/*设置所有信号  处理层面是进程层面*/
	assert(sigaction(sig,&sa,NULL) != -1);
}
int main(int argc,char *argv[])
{
	if(argc <= 2)
	{
		printf("usage: %s ip_address port_number\n",basename(argv[0]));
		return 1;
	}
	const char *ip = argv[1];
	int port = atoi(argv[2]);

	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port = htons(port);

	/*处理tcp 并将其绑定到端口port上*/
	int listenfd = socket(PF_INET,SOCK_STREAM,0);
	int reuse = 1;
	setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	assert(listenfd >= 0);
	int ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
	assert(ret != -1);
	ret = listen(listenfd,5);
	assert(ret != -1);


	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd != -1);
	/*监听socket listenfd上是不能注册EPOLLONESHOT事件的
	 * 否则程序只能处理一个客户连接 因为后续的客户端请求
	 * 不会再触发listenfd上的EPOLLIN事件*/
	addfd(epollfd,listenfd);

	/*使用socketpair创建管道，注册pipefd[0]上的可读时间*/
	ret = socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
	assert(ret  != -1);
	setnonblocking(pipefd[1]);
	addfd(epollfd,pipefd[0]);
	/*设置一些信号的处理函数*/
	addsig(SIGHUP);
	addsig(SIGCHLD);
	addsig(SIGTERM);
	addsig(SIGINT);
	bool stop_server = false;
	while(!stop_server)
	{
		int ret = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
		if(ret < 0 && errno != EINTR)/*这个错误是 当epoll正在被阻塞时 被其他信号中断了 就会有这情况*/
		{
			printf("epoll failure\n");
			break;
		}
		for(int i = 0;i < ret; ++i)
		{
			int sockfd = events[i].data.fd;
			if(sockfd == listenfd)
			{
				struct sockaddr_in client;
				socklen_t clientLen = sizeof(client);
				int connfd = accept(listenfd,(struct sockaddr*)&client,&clientLen);
				addfd(epollfd,connfd);
			}
			else if(sockfd == pipefd[0] && (events[i].events & EPOLLIN))
			{
				int sig;
				char signals[1024];
				ret = recv(pipefd[0],signals,sizeof(signals),0);
				if(ret == -1)
					continue;
				else if(ret == 0)
					continue;
				else
				{
					printf("%s\n",sig);
					/*因为每个信号值占1字节，所以按字节来逐个接受信号
					 * 我们以SIGTERM为例，来说明如何安全的终止服务器主循环*/
					for(int i = 0; i < ret; ++i)
					{
						switch(signals[i])
						{
							case SIGCHLD:
							case SIGHUP:
							{
								continue;
							}
							case SIGTERM:
							case SIGINT:
							{
								stop_server = true;
							}


						}
					}
				}
			}
			else
			{
				printf("something else happened\n");
			}
		}
	}
	printf("close fds\n");
	close(listenfd);
	close(pipefd[0]);
	close(pipefd[1]);
	return 0;
}
