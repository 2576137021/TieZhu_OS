#ifndef _USERPROG_PROCESS_H
#define _USERPROG_PROCESS_H
#include "../thread/thread.h"
#define USER_VADDR_START 0x8048000
/*
功能:激活页表,更新tss中的esp0位进程的特权级0栈
*/
void process_activate(task_struct* p_thread);
/*
功能:创建用户进程
参数:filename,用户进程起始地址,进程名称
*/
void process_execute(void* filename,char* name);
/*
功能:修改内核线程和用户进程的CR3
*/
void page_dir_activate(task_struct *p_thread);
/*
功能：创建用户进程所需的页目录表,并且复制内核空间的PDE,实现高1G共同映射内核
返回:成功返回页目录的虚拟地址。失败返回NULL
*/
uint32_t* create_page_dir(void);
#endif