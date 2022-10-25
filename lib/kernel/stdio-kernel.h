#ifndef _LIB_KERNEL_STDIO_KERNEL_H
#define _LIB_KERNEL_STDIO_KERNEL_H
//内核输出函数(不调用sys_write,直接操作显存)
void dbg_printf(const char* format,...);
#endif