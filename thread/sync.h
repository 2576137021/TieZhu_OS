#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "../lib/kernel/list.h"
#include "../lib/kernel/stdint.h"
#include "thread.h"
/*信号量结构*/
typedef struct _semaphore
{
    uint8_t value;
    list waiters;
}semaphore,*Psemaphore;
/*锁结构*/
typedef struct _lock
{
    task_struct* holder; //锁的任务持有者
    semaphore semaphore; //二元信号量默认初始值为1
    uint32_t holder_repeat_nm;//锁的持有者重复申请锁的次数(避免重复上锁对semaphore的影响)
}lock;
/*
功能:释放锁 
参数:pLock--锁指针
注释:非原子操作
*/
void lock_release(lock* pLock);
/*
功能:获取锁
参数:pLock--锁指针
注释:非原子操作
*/
void lock_acquire(lock* pLock);
/*初始化锁*/
void lock_init(lock* pLock);
/*
功能:初始化信号量
参数:信号量结构体指针,初始化的值(规定为1)
注释:原子操作
*/
void seam_init(semaphore* pSema,uint8_t value);
/*
功能:信号量down
参数:信号量结构体指针
注释:原子操作
*/
void sema_down(semaphore* psema);
/*
功能:信号量up
参数:信号量结构体指针
注释:原子操作
*/
void sema_up(semaphore* pSeamphore);
#endif