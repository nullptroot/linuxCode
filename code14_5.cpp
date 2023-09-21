/* ************************************************************************
> File Name:     code14_5.cpp
> Author:        程序员Boy
> 微信公众号:    
> Created Time:  2023年08月21日 星期一 19时20分38秒
> Description:   书上p286 页用一个线程处理所有信号
 ************************************************************************/
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

#define handle_error_en(en,msg) \
    do {errno = en;perror(msg);exit(EXIT_FAILURE);} while(0)

static void *sig_thread(void *arg)
{
    sigset_t *set = (sigset_t *)arg;
    int s,sig;
    for(;;)
    {
        /*第二个步骤调用sigwait等待信号*/
        s = sigwait(set,&sig);
        if(s != 0)
            handle_error_en(s,"sigwait");
        printf("Signal handing thread got signal %d\n",sig);
    }
}
int main(int argc,char *argv[])
{
    pthread_t thread;
    sigset_t set;
    int s;
    /*第一个步骤在主线程中设置信号掩码*/
    sigemptyset(&set);
    sigaddset(&set,SIGQUIT);
    sigaddset(&set,SIGUSR1);
    s = pthread_sigmask(SIG_BLOCK,&set,NULL);
    if(s != 0)
        handle_error_en(s,"pthread_sigmask");
    s = pthread_create(&thread,NULL,&sig_thread,(void *)&set);
    if(s != 0)
        handle_error_en(s,"pthread_create");
    pause();
}
