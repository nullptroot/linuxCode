#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static bool stop = 0;

static void handle_term(int sig)
{
	stop = 1;
}

int main(int argc,char *argv[])
{
	signal(SIGTERM,handle_term);//set signal SIGTERM'handle function
	if(argc <= 2)
	{
		printf("usage: %s ip_address port_number backlog\n",basename(argv[0]));
		return 1;
	}
	const char *ip = argv[1];
	int port = atoi(argv[2]);//atio is char * to int;

	int sock = socket(PF_INET,SOCK_STREAM,0);// PF_INET net server PF_UNIX local server,SOCK_STREAM tcp SOCK_DGRAM udp
	assert(sock >= 0);

	//create ipv4 socket address
	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port = htons(port);

	int ret = bind(sock,(struct sockaddr *)&address,sizeof(address));
	assert(ret != -1);//0 exit 1 nomal
	
	ret = listen(sock,5);
	assert(ret != -1);

	sleep(30);
	 
	struct sockaddr_in client;
	socklen_t client_addrlen = sizeof(client);
	int connfd = accept(sock,(struct sockaddr *)&client,&client_addrlen);
	if(connfd < 0)
		printf("error is %d\n",errno);
	else
	{
		char remote[INET_ADDRSTRLEN];
		printf("connected with ip: %s and port: %d\n",inet_ntop(AF_INET,&client.sin_addr,remote,INET_ADDRSTRLEN),ntohs(client.sin_port));
		close(connfd);
	}
	close(sock);
	return 0;
	
}
