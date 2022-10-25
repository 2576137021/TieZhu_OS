#ifndef _USERPROG_EXEC_H
#define _USERPROG_EXEC_H
#include "../lib/kernel/stdint.h"
/**
 * @brief 
 * 用path指向的程序替换当前进程
 * 成功不返回，失败返回 -1
 */
int32_t sys_exec(const char* path, const char* argv[]);
#endif