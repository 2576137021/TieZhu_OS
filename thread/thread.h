#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/memory.h"
//pcb文件描述符数组最大容量,一个描述符描述一个文件
#define MAX_FILES_OPEN_PER_PROC 8
#define TASK_NAME_LEN 16
extern list thread_ready_list;     //就绪链表
extern list thread_all_list;       //所有线程链表
/*
自定义通用函数类型,用于创建线程中的线程函数形参
*/
typedef void thread_func(void*);
/*
线程和进程状态
*/
enum task_status{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED   //即将回收的线程
};
/*
中断栈 intr_stack
用于保存中断发生时,执行流的上下文环境

思考:
    使用 intr_stack 记录中断发生的时候的栈内的各项的值,此结构体的定义,根据kernle.s的的中断中转
    程序的入栈顺序,并且将考虑到提权的情况。
    pushad入栈由先到后:
        eax ecx edx ebx esp ebp esi edi
    cpu自动压入(考虑到提权):
        push old_ss
        push old_esp
        push eflags
        push old_cs
        push old_eip
        push error_code //此项包括kernel.s中的(%2 ;传入的宏定义)
    kernel.s压入:
        push ds
        push es
        push fs
        push gs
        pushad

        push %1

    注意:
        结构体的地址是由低到高
        栈的地址是由高到低,注意顺序
*/
typedef struct _intr_stack
{   

    uint32_t vec_no; //kernel.s中压入的中断号

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t error_code;
    void (*old_eip)(void); //创建线程或进程时,此处栈中保存(线程)方法起始地址
    uint32_t old_cs;
    uint32_t eflags;
    void* old_esp;
    uint32_t old_ss;
}intr_stack,*pintr_stack;
/*
线程栈,存储线程中待执行的函数
前面四个寄存器是根据ABI规定(gcc编译器遵循此规定),由被调用者保存这四个寄存器
*/
typedef struct _thread_stack
{
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    /*
    当我们在初始化线程栈结构体时,eip应该指向thread_func的地址
    之后应该指向switch_to返回的地址(应该是时间片耗尽前的下一个指令地址,后面代码未写只是猜测)
    */
    void (*eip)(thread_func* func,void* func_arg);
    
    void (*unused_retaddr);//线程初始化时,保存NULL(模拟call的push操作),后续用于保存进入中断前的switch_to retvaddr
    thread_func* function;//同上类似,参考_线程切换栈模拟图
    void* func_arg;//switch_to 返回时,指向中断向量
}thread_stack,*pthread_stack;
/*
进程线程的pcb
*/
typedef struct _task_struct
{
    uint32_t* self_kstack;  //指向内核栈
    int16_t pid;//pid
    enum task_status status;//进程或线程状态
    uint8_t priority; //优先级
    uint8_t ticks;//每次运行的滴答数
    char name[TASK_NAME_LEN];//进程或线程的名称
    uint32_t elapsed_ticks;//已运行的滴答数总和
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC]; //文件描述符数组
    list_elem general_tag;//线程就绪队列的节点
    list_elem all_list_tag;//所有线程队列的节点
    uint32_t* pgdir; //虚拟页目录表地址(CR3,内核线程为NULL)
    struct _virtual_pool userprog_vaddr; //用户进程的虚拟地址池,用于管理进程堆
    mem_block_desc u_block_desc[DESC_CNT];//用户进程内存块描述符数组
    uint32_t cwd_inode_nr; //当前工作目录
    int16_t parent_pid; //父进程pid
    int8_t exit_status; //进程结束时调用exit传入的参数
    uint32_t stack_magic;//魔数,检测栈溢出
}task_struct,*ptask_struct;

#define STACK_MAGIC 0x889978AF //堆栈检测魔数
/*
功能:创建线程并执行
*/
task_struct* thread_start(char* thread_name,uint8_t priority,thread_func* thread_start_addr,void* thread_arg);
/*
功能:获取当前线程pcb指针
*/
task_struct* get_running_thread_pcb(void);
/*初始化主线程和链两个链表*/
void init_main_thread(void);
/*
功能:从就绪链表中找寻下一个任务,并进行线程切换
*/
void schedule(void);
void init_thread(task_struct* pthread,char*name,uint8_t priority);
/*
功能:阻塞线程
*/
void thread_block(enum task_status state);
/*
功能:解除线程pthread阻塞
*/
void thread_unlock(task_struct* pthread);
/*
功能:从线程内核栈中划分出中断栈和线程栈,并且初始化线程栈
*/
void thread_create(task_struct* thread_pcb,thread_func* func,void* func_arg);
/*
线程链表
*/
/**
 * @brief 
 * 让出cpu使用权,调用线程切换
 */
void thread_yield(void);
/**
 * @brief 
 * 返回一个pid
 */
int16_t fork_pid(void);
/**
 * @brief 
 * 输出全局线程信息
 */
void sys_ps(void);
/*释放pid*/
void release_pid(pid_t pid);
task_struct* pid2thread(int32_t pid);
/**
 * @brief 
 * 回收thread_over的pcb和页表，并将其从调度队列中去除
 */
void thread_exit(task_struct* thread_over,bool need_schedule);
#endif
