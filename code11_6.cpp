/* ************************************************************************
> File Name:     code11_6.cpp
> Author:        程序员Boy
> 微信公众号:    
> Created Time:  2023年07月11日 星期二 18时31分32秒
> Description:   时间堆定时器
 ************************************************************************/
#ifndef MIN_HEAP
#define MIN_HEAP

#include <iostream>
#include <netinet/in.h>
#include <time.h>
using std::exception;

#define BUFFER_SIZE 64

class heap_timer;/*前向声明*/
/*绑定socket和定时器*/
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer *timer;
};
/*定时器类*/
class heap_timer
{
public:
    heap_timer(int delay):expire(time(NULL) + delay){};
public:
        time_t expire;/*定时器生效得绝对时间*/
        void (*cb_func)(client_data *);/*定时器回调函数*/
        client_data *user_data;/*用户数据*/
};
class time_heap
{
public:
    /*构造函数之一，初始化一个大小为cap得空堆*/
    time_heap(int cap) throw(std::exception):capacity(cap),cur_size(0)
    {
        array = new heap_timer *[capacity];/*创建堆数组*/
        if(!array)
            throw std::exception;
        for(int i = 0; i < capacity; ++i)
            array[i] = nullptr;
    }
    /*构造函数之二，用已有得数组来初始化堆*/
    time_heap(heap_timer **init_array,int size,int capacity) throw (std::exception):cur_size(size),capacity(capacity)
    {
        if(capacity < size)
            throw (std::exception);
        array = new heap_timer *[capacity];/*创建堆数组*/
        if(!array)
            throw (std::exception);
        for(int i = 0; i < capacity; ++i)
            array[i] = nullptr;
        if(size != 0)
        {
            /*初始化数组*/
            for(int i = 0; i < size; ++i)
                array[i] = init_array[i];
            for(int i = (cur_size-1)/2; i >= 0; --i)
            {
                /*堆数组中得第[(cur_size-1)/2] ~ 0 个元素执行下滤操作*/
                percolate_down(i);
            }
        }
    }
    /*销毁时间堆*/
    ~time_heap()
    {
        for(int i = 0; i < cur_size; ++i)
            delete array[i];//初始化分配得也是指针，所以需要对其单个进行delete
        delete [] array;
    }
    /*添加目标定时器timer*/
    void add_timer(heap_timer *timer) throw (std::exception)
    {
        if(!timer)
            return;
        if(cur_size >= capacity)/*如果当前堆数组容量不够，则将其扩大一倍*/
            resize();
        /*新插入了一个元素，当前堆大小加1，hole是新建空穴的位置*/
        int hole = cur_size++;
        int parent = 0;
        /*对从空穴到根节点的路经上的所有节点执行上滤操作*/
        for(;hole > 0; hole = parent)
        {
            parent = (hole - 1)/2;
            if(array[parent]->expire <= timer->expire)
                break;
            array[hole] = array[parent];
        }
        array[hole] = timer;
        /*heapInsert操作*/
    }
    /*删除目标定时器timer*/
    void del_timer(heap_timer *timer)
    {
        if(!timer)
            return;
        /*仅仅将目标定时器的回调函数设置为nullptr，即所谓的延迟销毁
         * 这种节省真正删除该定时器造成的开销，这样做容易使堆数组膨胀*/
        timer->cb_func = nullptr;
        /*没有真正的删除定时器，只是它不能调用函数了*/
    }
    /*获得堆顶的定时器*/
    heap_timer *top() const
    {
        if(empty())
            return nullptr;
        return array[0];
    }
    /*删除栈顶的定时器*/
    void pop_timer()
    {
        if(empty())
            return;
        if(array[0])
        {
            delete array[0];
            /*将原来的堆顶元素替换为堆数组种最后一个元素*/
            array[0] = array[--cur_size];
            percolate_down(0);/*对新顶执行下滤操作*/
        }
    }
    void tick()
    {
        heap_timer *tmp = array[0];
        time_t cur = time(NULL);/*循环处理堆中到期的定时器*/
        while(!empty())
        {
            if(!tmp)
                break;    
            /*如果定时器其没有到期，则推出循环*/
            if(tmp->expire > cur)
                break;
            /*否则执行堆顶定时器任务*/
            if(tmp->cb_func)
                tmp->cb_func(tmp->user_data);
            /*将栈顶元素删除，同时生成新的栈顶定时器*/
            pop_timer();
            tmp = array[0];
        }
    }
    bool empty() const{
        return cur_size == 0;
    }
private:
    /*最小堆的下滤操作，它确保堆数组种以第hole个节点作为跟的子树拥有最小堆性质*/
    void percolate_down(int hole)
    {
        heap_timer *temp = array[hole];
        int child = 0;
        for(;(hole * 2 + 1) <= cur_size - 1; hole = child)
        {
            child = hole * 2 + 1;
            if(child < (cur_size - 1) && array[child + 1]->expire < array[child]->expire)
                ++child;
            if(array[child]->expire < temp->expire)
                array[hole] = array[child];
            else
                break;
        }
        array[hole] = temp;
    }
    /*将堆数组容量扩大一倍*/
    void resize() throw (std::exception)
    {
        heap_timer **temp = new heap_timer *[2 * capacity];
        /*有点错误的  判断非空应该在 遍历之前*/
        if(!temp)
            throw std::exception();
        for(int i = 0; i < cur_size; ++i)
            temp[i] = nullptr;
        
        capacity = capacity * 2;
        for(int i = 0; i < cur_size; ++i)
            temp[i] = array[i];
        delete [] array;
        array = temp;
    }
    heap_timer **array;/*堆数组*/
    int capacity;/*队数组的容量*/
    int cur_size;/*堆数组当前包含元素得个数*/
}

#endif
