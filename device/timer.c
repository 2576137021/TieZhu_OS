#include "timer.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/interrupt.h"
#include "../thread/thread.h"
#include "../lib/kernel/global.h"
#define m_seconds_per_intr 10 //多少毫秒发送一次中断
uint32_t ticks;//内核运行到现在,总共的滴答数
/*定时器中断处理方法*/
static void intr_timer_handler(void){
    task_struct* thread_pcb =  get_running_thread_pcb();
    assert(thread_pcb->stack_magic==STACK_MAGIC); //检测堆栈溢出
    thread_pcb->elapsed_ticks++;
    ticks++;//内核运行到现在,总共的滴答数
    if(thread_pcb->ticks==0){
        //运行时间结束
        schedule();
    }else{
        thread_pcb->ticks--;
    }
    return;
}
/*初始化定时器*/
void init_timer(){
    //初始化控制字
    int8_t controlByte = COUNTER_0<<6+COUNTER_rw_mode<<4+COUNTER_job_mode<<1+COUNTER_binary_mode;
    //为8253计数器0写入初始值
    int16_t start_value = COUNTER_START_VALUE;
    outb(R8253_CONTRNL_BYTE,controlByte);
    //先写低8位
    outb(R8253_COUNTER,controlByte&0xff);
    outb(R8253_COUNTER,(controlByte&0xff00)>>8);
    //添加定时器中断处理例程
    register_handler(0x20,intr_timer_handler);
    print("init_timer sucess\n");
}
/**
 * @brief 
 * 以滴答声为单位的sleep
 */
static void ticks_to_sleep(uint32_t sleep_ticks){
    uint32_t strart_tick = ticks;
    while (strart_tick+sleep_ticks<ticks)
    {
        thread_yield();
    }
    
}
/**
 * @brief 
 * 以毫秒为单位的sleep
 */
void sleep(uint32_t mseconds){
    uint32_t sleep_ticks = DIV_ROUND_UP(mseconds,10); //转换为滴答数
    ticks_to_sleep(sleep_ticks);
}
