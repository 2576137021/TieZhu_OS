#ifndef _USERPROG_WAIT_EXIT_H
#define _USERPROG_WAIT_EXIT_H
#include "../lib/kernel/stdint.h"
/**
 * @brief 
 * 子进程结束时调用,status退出状态
 */
void sys_exit(int32_t status);
/**
 * @brief 
 * 等待子进程调用exit,将子进程的退出状态保存到status中,status可以为NULL
 * 成功返回子进程的pid，失败返回-1
 */
pid_t sys_wait(int32_t* status);

#endif
