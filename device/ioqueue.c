#include"ioqueue.h"
#include"../lib/kernel/debug.h"
#include"../lib/kernel/interrupt.h"
#include"../lib/kernel/print.h"
/*
初始化ioq队列
*/
void ioqueue_init(ioqueue* ioq){
    ioq->head =ioq->tail=0;
    lock_init(&ioq->lock);
    ioq->producer=ioq->consumer=NULL;
    //test
    print("ioqueue_init success\n");
}
/*
返回指针pos的下一个位置
*/
static uint32_t next_pos(uint32_t pos){
    return (pos+1)%buffer_size; //这里就非常的灵性
}
/*
判断队列ioq是否已满
*/
bool ioq_full(ioqueue* ioq){
    return next_pos(ioq->head)==ioq->tail;
}
/*
判断队列ioq是否为空
*/
bool ioq_empty(ioqueue* ioq){
    return ioq->head==ioq->tail;
}
/*
使当前的消费者或生产者(wait_task)在此缓冲区上等待
*/
static void ioq_wait(task_struct** wait_task){
    enum intr_status old_state = intr_disable();
    *wait_task = get_running_thread_pcb(); //记录被休眠的线程(必须在前面,否则设置线程阻塞后没法记录,就没法解锁),因为这个函数已经被我上锁了,所以不怕被打断了.被坑了半小时。
    ioqueue* tempIoq = (ioqueue*)struct_entry(wait_task,ioqueue,consumer);
    thread_block(TASK_BLOCKED);
    intr_set_status(old_state);
}
/*
唤醒等待在此缓冲区上的消费者或生产者线程(wait_task)
*/
static void ioq_wakeup(task_struct** wakeup_task){
    enum intr_status old_state = intr_disable();
    thread_unlock(*wakeup_task);
    *wakeup_task=NULL; //删除被休眠的线程记录
    intr_set_status(old_state);
}
/*
消费者从ioq队列中获取一个字节
*/
char ioq_getchar(ioqueue* ioq){
    enum intr_status old_status = intr_disable();
    assert(get_intr_status()==INTR_OFF);//保证是互斥操作
    /*如果队列为空*/
    while (ioq_empty(ioq))
    {   
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);//因为队列中无值可取,所以一直等待到有值可取
        lock_release(&ioq->lock);
    }
    char byte =ioq->buffer[ioq->tail];
    ioq->tail = next_pos(ioq->tail);
    if(ioq->producer!=NULL){
        ioq_wakeup(&ioq->producer); //如果存在生产者唤醒生产者线程
    }
    intr_set_status(old_status);
    return byte;
}
/*
生产者向ioq队列中写入一个字符
*/
void ioq_putchar(ioqueue* ioq,char byte){
    enum intr_status old_status = intr_disable();
    assert(get_intr_status()==INTR_OFF);//保证是互斥操作
    while (ioq_full(ioq))
    {   
       //队列已满,不再写入,等待消费者获取
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }
    ioq->buffer[ioq->head] =byte;
    ioq->head = next_pos(ioq->head);
    if(ioq->consumer!=NULL){
        ioq_wakeup(&ioq->consumer);
    }
    intr_set_status(old_status);
    return;
}
/**
 * @brief 
 * 返回环形缓冲区中的数据长度
 */
uint32_t ioq_length(ioqueue* ioq){
    uint32_t len = 0;
    if(ioq->head >= ioq->tail){
        len = ioq->head - ioq->tail;
    }else{
        len = buffer_size-(ioq->tail-ioq->head);
    }
    return len;
}

