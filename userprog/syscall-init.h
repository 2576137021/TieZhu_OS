#ifndef _USERPROG_SYSCALL_INIT_H
#define _USERPROG_SYSCALL_INIT_H
#include "../lib/kernel/stdint.h"
/*初始化系统调用*/
void syscall_init(void);
/*写文件*/
uint32_t write(int32_t fd,const void* buf,uint32_t count);
/*返回当前任务pid*/
uint32_t sys_getpid(void);
#endif