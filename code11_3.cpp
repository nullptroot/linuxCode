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
#include "code11_2.h"


#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
/*利用代码清单11-2中的升序链表来管理定时器*/
static sort_timer_lst timer_lst;
static int epollfd = 0;

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
void timer_handler()
{
	/*定时处理任务，实际上就是调用tick函数*/
	timer_lst.tick();
	/*因为一次alarm调用只会引起一次SIGALRM信号，所以我们需要重新定时
	 * 以不断的触发SIGALRM信号*/
	alarm(TIMESLOT);
}
/*定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之*/
void cb_func(client_data *user_data)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
	assert(user_data);
	close(user_data->sockfd);
	printf("close fd %d\n",user_data->sockfd);
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
	addsig(SIGALRM);
	addsig(SIGTERM);
	bool stop_server = false;

	client_data *users = new client_data[FD_LIMIT];
	bool timeout = false;
	alarm(TIMESLOT);/*定时*/

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
			/*处理新到的客户连接*/
			if(sockfd == listenfd)
			{
				struct sockaddr_in client;
				socklen_t clientLen = sizeof(client);
				int connfd = accept(listenfd,(struct sockaddr*)&client,&clientLen);
				addfd(epollfd,connfd);
				users[connfd].address = client;
				users[connfd].sockfd = connfd;
				/*创建定时器 设置其回调函数与超时函数，然后绑定定时器与用户数据
				 * 最后将定时器添加到链表timer_lst中*/
				util_timer *timer = new util_timer;
				timer->user_data = &users[connfd];
				timer->cb_func = cb_func;
				time_t cur = time(NULL);
				timer->expire = cur + 3 * TIMESLOT;
				users[connfd].timer = timer;
				timer_lst.add_timer(timer);
			}
			/*处理信号*/
			else if(sockfd == pipefd[0] && (events[i].events & EPOLLIN))
			{
				char signals[1024];
				ret = recv(pipefd[0],signals,sizeof(signals),0);
				if(ret == -1)
					continue;
				else if(ret == 0)
					continue;
				else
				{
					/*因为每个信号值占1字节，所以按字节来逐个接受信号
					 * 我们以SIGTERM为例，来说明如何安全的终止服务器主循环*/
					for(int i = 0; i < ret; ++i)
					{
						switch(signals[i])
						{
							case SIGALRM:
							{
								/*用timeout变量标记有定时任务需要处理，但是不立即处理定时任务
								 * 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的
								 * 任务*/
								timeout = true;
								break;
							}
							case SIGTERM:
							{
								stop_server = true;
							}


						}
					}
				}
			}
			/*处理客户连接上接受到的数据*/
			else if(events[i].events & EPOLLIN)
			{
				memset(users[sockfd].buf,'\0',BUFFER_SIZE);
				ret = recv(sockfd,users[sockfd].buf,BUFFER_SIZE - 1,0);
				printf("get %d bytes of client data %s from %d\n",ret,users[sockfd].buf,sockfd);
				util_timer *timer = users[sockfd].timer;
				if(ret < 0)
				{
					/*发生读错误，关闭连接，并移除其对应的定时器*/
					if(errno != EAGAIN)
					{
						cb_func(&users[sockfd]);
						if(timer)
							timer_lst.del_timer(timer);
					}
				}
				else if(ret == 0)
				{
					/*如果对方已经关闭连接，则我们也关闭连接，并移除对应的定时器*/
					cb_func(&users[sockfd]);
					if(timer)
						timer_lst.del_timer(timer);
				}
				else
				{
					/*如果某个客户连接上有数据可读，则我们需要调整改连接对应的定时器
					 * 以延迟改连接被关闭的事件*/
					if(timer != nullptr)
					{
						time_t cur = time(NULL);
						timer->expire = cur + 3 * TIMESLOT;
						printf("adjust timer once\n");
						timer_lst.adjust_timer(timer);
					}
				}
			}
			else
			{

			}
			if(timeout)
			{
				timer_handler();
				timeout = false;
			}
		}

	}
	close(listenfd);
	close(pipefd[0]);
	close(pipefd[1]);
	delete [] users;
	return 0;
}
