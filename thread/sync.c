#include "sync.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/print.h"

/*
功能:初始化信号量
参数:信号量结构体指针,初始化的值(规定为1)
注释:原子操作
*/
void seam_init(semaphore* pSema,uint8_t value){
    pSema->value = value;
    list_init(&pSema->waiters);
}
/*初始化锁*/
void lock_init(lock* pLock){
    pLock->holder = NULL;
    pLock->holder_repeat_nm = 0;
    seam_init(&pLock->semaphore,1);
}
/*
功能:信号量down
参数:信号量结构体指针
注释:原子操作
*/
void sema_down(semaphore* psema){
    enum intr_status old_status = intr_disable();
    task_struct* pthread = get_running_thread_pcb();
    //因为我们释放锁的时候会将等待线程添加到就序列表的队首,所以可以使用if
    while(psema->value==0){
        //如锁已被取走,则将当前线程加入此锁的等待链表中
        if(!list_elem_find(&psema->waiters,&pthread->general_tag)){
            list_append(&psema->waiters,&pthread->general_tag);
            //阻塞线程
            thread_block(TASK_BLOCKED);
        }
    }
    /*如果value==1,或者线程被唤醒则继续执行*/
    psema->value--;
    assert(psema->value==0);
    intr_set_status(old_status);
}
/*
功能:信号量up
参数:信号量结构体指针
注释:原子操作
*/
void sema_up(semaphore* pSeamphore){
    enum intr_status old_status = intr_disable();
    assert(pSeamphore->value==0);
    if(!list_isempty(&pSeamphore->waiters)){
        //如果就绪链表不为空
        task_struct* pthread  = (task_struct*)struct_entry(list_pop(&pSeamphore->waiters),task_struct,general_tag);
        //恢复等待线程的运行
        thread_unlock(pthread);
    }
    pSeamphore->value+=1;
    intr_set_status(old_status);
}
/*
功能:获取锁
参数:pLock--锁指针
注释:非原子操作
*/
void lock_acquire(lock* pLock){
    if(pLock->holder!=get_running_thread_pcb()){
        //锁的持有者不是当前线程,或者锁不存在持有者
        sema_down(&pLock->semaphore);
        //在获取到锁之后,更改锁的持有者位当前线程
        pLock->holder = get_running_thread_pcb();
        pLock->holder_repeat_nm = 1;
    }else{
        //锁的持有者是当前线程
        pLock->holder_repeat_nm++;
    }
}
/*
功能:释放锁 
参数:pLock--锁指针
注释:非原子操作
*/
void lock_release(lock* pLock){
    assert(pLock->holder==get_running_thread_pcb());
    if(pLock->holder_repeat_nm>1){
        pLock->holder_repeat_nm--;
        return;
    }
    //print("\n***\nunLock sucess\n");
    pLock->holder_repeat_nm = 0;
    pLock->holder = NULL;
    //增加信号量
    sema_up(&pLock->semaphore);

}
