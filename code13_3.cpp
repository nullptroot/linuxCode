/* ************************************************************************
> File Name:     code13_3.cpp
> Author:        程序员Boy
> 微信公众号:    
> Created Time:  2023年07月14日 星期五 18时23分53秒
> Description:   使用IPC——PRIVATE信号量
 ************************************************************************/
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
    struct seminfo *_buf;
};
/*op为-1时执行p操作，op为1时执行v操作*/
void pv(int sem_id,int op)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = op;
    sem_b.sem_flg = SEM_UNDO;
    semop(sem_id,&sem_b,1);
}
int main()
{
    int sem_id = semget(IPC_PRIVATE,1,0666);
    union semun sem_un;
    sem_un.val = 1;
    semctl(sem_id,0,SETVAL,sem_un);

    pid_t id = fork();
    if(id < 0)
        return 1;
    else if(id == 0)
    {
        printf("child try to get binay sem\b");
        /*在父子进程间共享IPC_PRIVATE 信号量的关键在于两者都有
         * 可以操作该信号量的标识符，sem_id*/
        pv(sem_id,-1);
        printf("child get the sem and would release it after 5 seconds\n");
        sleep(5);
        pv(sem_id,1);
        exit(0);
    }
    else
    {
        printf("parent try to get binay sem\n");
        pv(sem_id,-1);
        printf("parent get the sem and would release it after 5 seconds\n");
        sleep(5);
        pv(sem_id,1);
    }
    waitpid(id,NULL,0);
    semctl(sem_id,0,IPC_RMID,sem_un);/*删除信号量*/
    return 0;
}
