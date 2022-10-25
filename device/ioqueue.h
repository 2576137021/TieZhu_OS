#ifndef DEVICE_IOQUEUE_H
#define DEVICE_IOQUEUE_H
#include "../lib/kernel/stdint.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
#define buffer_size 64//缓冲区大小

/*
环形队列
*/
typedef struct _ioqueue
{
    lock lock;
    task_struct * producer; //记录等待的生产者线程
    task_struct * consumer; //记录等待的消费者线程
    char buffer[buffer_size];
    uint32_t head; //队首,数据写入队首位置
    uint32_t tail; //队尾,数据从队尾开始读取
}ioqueue;
/*
生产者向ioq队列中写入一个字符
*/
void ioq_putchar(ioqueue* ioq,char byte);
/*
消费者从ioq队列中获取一个字符
*/
char ioq_getchar(ioqueue* ioq);
/*
初始化ioq队列
*/
void ioqueue_init(ioqueue* ioq);
/*
判断队列ioq是否已满
*/
bool ioq_full(ioqueue* ioq);
/*
判断队列ioq是否为空
*/
bool ioq_empty(ioqueue* ioq);
/**
 * @brief 
 * 返回环形缓冲区中的数据长度
 */
uint32_t ioq_length(ioqueue* ioq);
#endif