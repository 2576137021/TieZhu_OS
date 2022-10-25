#ifndef _LIB_TIMER_H
#define _LIB_TIMER_H
//计数器控制字节
#define COUNTER_0 0             //计数器0
#define COUNTER_rw_mode 3       //写入或读取数据的方式
#define COUNTER_job_mode 2      //工作方式
#define COUNTER_binary_mode 0   //数制格式 = 二进制
#define R8253_CONTRNL_BYTE 0x43 //8253控制字端口
#define R8253_COUNTER 0x40      //8253计数器0的端口
#define COUNTER_START_VALUE   1193180/100  //计数器的初值,初值越大,中断频率越低,初值越低,中断频率越高.
#include "../lib/kernel/stdint.h"
/*
    8253的工作频率为1.19318MHz = 1193180Hz,即每秒发生1193180次脉冲信号
    8253每收到一次信号,初值-1。最大初值为0x0(0xfff第二大,8253计数器的规则) = 65536,需要进行65536次操作
    1193180/工作频率 = 工作效率(中断信号发送频率)
*/
void init_timer(void);
/**
 * @brief 
 * 以毫秒为单位的sleep
 */
void sleep(uint32_t mseconds);
#endif