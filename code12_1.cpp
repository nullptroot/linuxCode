/* ************************************************************************
> File Name:     code12_1.cpp
> Author:        程序员Boy
> 微信公众号:    
> Created Time:  2023年07月13日 星期四 18时11分48秒
> Description:   一个使用libevent库的程序 hello world
 ************************************************************************/
#include <sys/signal.h>
#include <event.h>

void signal_cb(int fd,short event,void *argc)
{
    struct event_base *base = (event_base *)argc;
    struct timeval delay = {
        2,0
    };
    printf("Caught an interrupt signal;exiting cleanly in two seconds...\n");
    event_base_loopexit(base,&delay);
}
void timeout_cb(int fd,short event,void *argc)
{
    printf("timerout\n");
}

int main()
{
    struct event_base *base = event_init();
    struct event *signal_event = evsignal_new(base,SIGINT,signal_cb,base);
    event_add(signal_event,NULL);
    timeval tv = {
        1,0
    };
    struct event *timerout_event = evtimer_new(base,timeout_cb,NULL);
    event_add(timerout_event,&tv);
    event_base_dispatch(base);
    event_free(timerout_event);
    event_free(signal_event);
    event_base_free(base);
}
