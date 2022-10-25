#ifndef _USERPROG_FORK_H
#define _USERPROG_FORK_H
#include "../lib/kernel/stdint.h"
/**
 * @brief 
 * 克隆用户进程
 * 成功父进程返回子进程pid,子进程返回0,失败返回-1,
 */
pid_t sys_fork(void);
#endif