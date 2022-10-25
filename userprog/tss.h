#ifndef _USERPROG_TSS_H
#define _USERPROG_TSS_H
#include "../thread/thread.h"
/*
功能:在gdt中创建tss并重新加载gdt
*/
void tss_init(void);
/*
功能:更新tss中的esp0字段的值为pthread的0级栈
*/
void update_tss_esp(task_struct* pthread);
#endif