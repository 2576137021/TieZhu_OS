#include "thread.h"
#include "sync.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../lib/kernel/print.h"
#include "../userprog/process.h"
#include "../lib/stdio.h"
#include "../fs/fs.h"

task_struct* main_thread_struct;   //内核主线程pcb
list thread_ready_list;     //就绪链表
list thread_all_list;       //所有线程链表
static list_elem* thread_tag; //线程交换中用户接收链表元素的值
task_struct* idle_thread; //空闲线程
extern void switch_to(task_struct* cut,task_struct* next);
extern void init(void);
static void make_main_thread(void);
/*pid的位图,最大支持1024个pid*/
uint8_t pid_bitmap_bits[128] = {0};
/*pid池*/
struct _pid_pool{
    BitMap pid_bitmap;//pid位图
    uint32_t pid_start;//起始pid
    lock pid_lock; //分配pid锁
};
struct _pid_pool pid_pool;
/*初始化pid池*/
static void pid_pool_init(void){
    pid_pool.pid_bitmap.bits = pid_bitmap_bits;
    pid_pool.pid_bitmap.bitmap_byte_len = 128;
    pid_pool.pid_start = 1;
    bitmap_init(&pid_pool.pid_bitmap);
    lock_init(&pid_pool.pid_lock);
}
/*系统空闲时运行的线程*/
static void idle(void){
    while (true)
    {
        thread_block(TASK_BLOCKED);
        //使计算机停机,保证在开中断下运行
        asm volatile("sti;hlt":::"memory");
    }
    
}

void init_main_thread(void){
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    pid_pool_init();//初始化pid

    //创建第一个用户进程:init
    process_execute(init,"init");

    //创建内核主线程
    make_main_thread();
    //创建idle线程
    idle_thread = thread_start("idle",10,(thread_func *)idle,NULL);
    print("init_main_thread success\n");
}
/*
功能:分配pid
*/
static int16_t allocate_pid(void){
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = bitMap_scan(&pid_pool.pid_bitmap,1);
    assert(bit_idx!=-1);
    bitmap_set(&pid_pool.pid_bitmap,bit_idx,1);
    lock_release(&pid_pool.pid_lock);
    return (bit_idx + pid_pool.pid_start);
}
/*释放pid*/
void release_pid(pid_t pid){
    lock_acquire(&pid_pool.pid_lock);
    int32_t pid_idx = pid-pid_pool.pid_start;
    bitmap_set(&pid_pool.pid_bitmap,pid_idx,0);
    lock_release(&pid_pool.pid_lock);
}
/**
 * @brief 
 * 回收thread_over的pcb和页表，并将其从调度队列中去除
 */
void thread_exit(task_struct* thread_over,bool need_schedule){
    /*关闭中断*/
   enum intr_status intr_state = intr_disable();
   thread_over->status = TASK_DIED;

   //在链表中去除进程
   if(list_elem_find(&thread_ready_list,&thread_over->general_tag)){
        list_remove(&thread_over->general_tag);
   }
   if(list_elem_find(&thread_all_list,&thread_over->all_list_tag)){
        list_remove(&thread_over->all_list_tag);
   }
   //回收进程的页表
   if(thread_over->pgdir){
        mfree_page(PF_KERNEL,thread_over->pgdir,1);
   }
    //释放pid
    release_pid(thread_over->pid);
    //回收pcb
   if(thread_over != main_thread_struct){
        mfree_page(PF_KERNEL,thread_over,1);
   }
    if(need_schedule)schedule();
}
/**
 * @brief 
 * 比较pid
 * pelem 所有线程链表节点
 */
static bool pid_check(list_elem* pelem,int32_t pid){
    task_struct* pthread = struct_entry(pelem,task_struct,all_list_tag);
    if(pthread->pid == pid)return true;
    return false;
}
/**
 * @brief 
 * 根据pid返回pcb
 * 成功返回pcb，失败返回null
 */
task_struct* pid2thread(int32_t pid){
    list_elem* pelem = list_traversal(&thread_all_list,(functionc*)pid_check,(void*)pid);
    if(pelem ==NULL)return NULL;
    task_struct* thread = struct_entry(pelem,task_struct,all_list_tag);
    return thread;
}
/*
功能:获取当前线程pcb指针
*/
task_struct* get_running_thread_pcb(void){
    uint32_t esp;
    asm volatile("mov %%esp,%0":"=g" (esp));
    return (task_struct*)(esp&0xfffff000);
}
/*
功能:初始化内核主线程pcb
*/
static void make_main_thread(void){
    //内核的esp为 0x9f000
    main_thread_struct = get_running_thread_pcb();
    init_thread(main_thread_struct,"main_thread",3);
    list_append(&thread_all_list,&main_thread_struct->all_list_tag);
}
/*
功能:执行新建线程的线程函数方法
*/
static void kernel_thread(thread_func* func,void* func_arg){
    //提前开启中断,用于在新的线程中可以进行任务切换
    intr_enable();
    func(func_arg);
}
/*
功能:从线程内核栈中划分出中断栈和线程栈,并且初始化线程栈
*/
void thread_create(task_struct* thread_pcb,thread_func* func,void* func_arg){
    
    //预留中断使用的栈空间
    thread_pcb->self_kstack -=sizeof(intr_stack);
    //留出线程栈空间
    thread_pcb->self_kstack -=sizeof(thread_stack);//初始化的线程esp指向 thread_stack 结构体
    //初始化线程栈
    thread_stack* kthread_stack = (thread_stack*)thread_pcb->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = func;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp=kthread_stack->ebx =\
    kthread_stack->edi=kthread_stack->esi = 0;
}
/*
初始化pcb
pcb和线程的内核栈都位于一页之中,pcb位于页的低地址,内核栈位于页的高地址
*/
void init_thread(task_struct* thread_pcb,char*name,uint8_t priority){
    memset(thread_pcb,0,sizeof(task_struct));
    strcpy(thread_pcb->name,name);
    if(thread_pcb==main_thread_struct){
        //主线程,设置运行状态
        thread_pcb->status = TASK_RUNNING;
    }else{
        //其他线程,设置为就绪状态
        thread_pcb->status = TASK_READY;
    }
    /*标准输入输出流*/
    thread_pcb->fd_table[0] = 0; 
    thread_pcb->fd_table[1] = 1;
    thread_pcb->fd_table[2] = 2;
    /*初始化其他文件描述符为 -1 */
    uint8_t id_x = 3;
    while (id_x<MAX_FILES_OPEN_PER_PROC)
    {
        thread_pcb->fd_table[id_x] = -1;
        id_x++;
    }
    thread_pcb->pid = allocate_pid();
    thread_pcb->priority = priority;
    thread_pcb->self_kstack = (uint32_t*)((uint32_t)thread_pcb+PG_SIZE); //内核栈
    thread_pcb->elapsed_ticks = 0;
    thread_pcb->pgdir = NULL;
    thread_pcb->ticks = priority; //运行时间 = 线程的优先级
    thread_pcb->cwd_inode_nr = 0;
    thread_pcb->parent_pid = -1; //没有父进程
    thread_pcb->stack_magic = STACK_MAGIC; //魔数,以后用于判断内核栈是否覆盖了pcb
 
}
/*
功能:创建线程
参数:线程名 thread_name,线程优先级 priority,
线程函数地址 thread_start_addr,线程函数参数 thread_arg
返回:线程信息结构体 task_strcut
*/
task_struct* thread_start(char* thread_name,uint8_t priority,thread_func* thread_start_addr,void* thread_arg){
    task_struct* taskStruct = (task_struct*)get_kernel_pages(1);
    //初始化pcb
    init_thread(taskStruct,thread_name,priority);
    //初始化中断栈和线程栈
    thread_create(taskStruct,thread_start_addr,thread_arg);
    
    //加入就绪链表和全部线程链表
    assert(!list_elem_find(&thread_all_list,&taskStruct->all_list_tag));
    assert(!list_elem_find(&thread_ready_list,&taskStruct->general_tag));
    list_append(&thread_all_list,&taskStruct->all_list_tag);
    list_append(&thread_ready_list,&taskStruct->general_tag);
    return taskStruct;
}
/*
功能:阻塞线程
*/
void thread_block(enum task_status state){
    //这里关中断,避免在已记录预测的锁状态,但锁状态还没有修改的情况下,被线程调度打断。
    enum intr_status old_state = intr_disable();
    //必须是以下三种状态才能被阻塞
    assert((state==TASK_BLOCKED)||(state==TASK_HANGING)||(state==TASK_WAITING));
    //进入线程调度后怎么恢复中断呢？
    task_struct* cur = get_running_thread_pcb();
    cur->status = state;
    schedule();
    intr_set_status(old_state);
}
/*
功能:解除线程pthread阻塞
*/
void thread_unlock(task_struct* pthread){
    enum intr_status old_status = intr_disable();
    assert((pthread->status==TASK_BLOCKED)||(pthread->status==TASK_HANGING)||(pthread->status==TASK_WAITING));
    assert(pthread->status!= TASK_READY);
    assert(!list_elem_find(&thread_ready_list,&pthread->general_tag));
    list_push(&thread_ready_list,&pthread->general_tag);
    pthread->status = TASK_RUNNING;
    intr_set_status(old_status);
}
/**
 * @brief 
 * 让出cpu使用权,调用线程切换
 */
void thread_yield(void){
    task_struct* cur_thread = get_running_thread_pcb();
    enum intr_status old_status = intr_disable();
    list_append(&thread_ready_list,&cur_thread->general_tag);
    cur_thread->status = TASK_READY;
    schedule();
    intr_set_status(old_status);
}
/**
 * @brief 
 * 返回一个pid
 */
int16_t fork_pid(void){
    return (pid_t)allocate_pid();
}
/**
 * @brief 
 * 右对齐打印输出
 */
static void pad_print(char* buf,int32_t buf_len,void* ptr,char format){
    memset(buf,0,buf_len);
    uint8_t out_pad_idx  = 0;
    switch(format){
        case 's':
            out_pad_idx = sprintf(buf,"%s",ptr);
            break;
        case 'd':
            out_pad_idx = sprintf(buf,"%d",*(int16_t*)ptr);
            break;
        case 'x':
            out_pad_idx = sprintf(buf,"0x%x",*(uint32_t*)ptr);
    }
    while(out_pad_idx < buf_len){
        buf[out_pad_idx] = ' ';
        out_pad_idx++;
    }

    sys_write(1,buf,buf_len-1);
}
/**
 * @brief 
 * list回调函数
 * 输出全局线程信息
 */
static bool elem2thread_info(list_elem* pelem,int32_t arg){
    task_struct * pthread = (task_struct *)struct_entry(pelem,task_struct,all_list_tag);
    
    char out_pad[16] = {0};
    pad_print(out_pad,16,&pthread->pid,'d');

    if(pthread->parent_pid == -1){
        pad_print(out_pad,16,"NULL",'s');
    }else{
        pad_print(out_pad,16,&pthread->parent_pid,'d');
    }
    switch(pthread->status){
        case 0:
            pad_print(out_pad,16,"RUNNING",'s');
            break;
        case 1:
            pad_print(out_pad,16,"READY",'s');
            break;
        case 2:
            pad_print(out_pad,16,"BLOCKED",'s');
            break;
        case 3:
            pad_print(out_pad,16,"WAITING",'s');
            break;
        case 4:
            pad_print(out_pad,16,"HANGING",'s');
            break;
        case 5:
            pad_print(out_pad,16,"DIDE",'s');
            break;
    }
    pad_print(out_pad,16,&pthread->elapsed_ticks,'x');


    memset(out_pad,0,16);
    assert(strlen(pthread->name) < 17);
    memcpy(out_pad,pthread->name,strlen(pthread->name));
    strcat(out_pad,"\n");
    sys_write(1,out_pad,strlen(out_pad));
    return false;
}
void sys_ps(void){
    char* ps_title = "PID            PPID           STAT           TICKS           COMMAND\n";
    sys_write(1,ps_title,strlen(ps_title));
    list_traversal(&thread_all_list,(functionc*)elem2thread_info,0);
}



/*
功能:任务调度
*/
void schedule(void){

    assert(get_intr_status()==INTR_OFF); //进入中断处理程序后,可屏蔽中断将会自动关闭,这里是检查一下
    task_struct* cur_thread = get_running_thread_pcb();
    if(cur_thread->status==TASK_RUNNING){
        assert(!list_elem_find(&thread_ready_list,&cur_thread->general_tag));
        //时间片到期
        list_append(&thread_ready_list,&cur_thread->general_tag);
        //恢复时间片
        cur_thread->ticks = cur_thread->priority;
        //设置线程就绪状态
        cur_thread->status = TASK_READY;
    }else{
        //其他情况被换下
    }
    if(list_isempty(&thread_ready_list)){
        //如果就绪链表为空，则唤醒idle线程
        thread_unlock(idle_thread);
    }
    thread_tag = NULL;
    thread_tag  = list_pop(&thread_ready_list);
    task_struct* next_thread = (task_struct*)struct_entry(thread_tag,task_struct,general_tag);

    //线程交换
    next_thread->status = TASK_RUNNING;
   // dbg_printf("schedule:now thread:0x%x,next thread:0x%x\n",cur_thread,next_thread);
    //修改cr3,修改tss的esp0
    process_activate(next_thread);
    switch_to(cur_thread,next_thread);
}
