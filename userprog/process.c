#include"process.h"
#include"../thread/thread.h"
#include"../lib/kernel/global.h"
#include"../lib/kernel/memory.h"
#include"../lib/kernel/debug.h"
#include"../lib/kernel/string.h"
#include"../lib/kernel/print.h"
#include"../lib/kernel/interrupt.h"
#include "../userprog/tss.h"
#include"../lib/kernel/stdio-kernel.h"

extern void intr_exit(void);

#define default_prio 3//用户进程优先级,给的比较低方便切换到其他线程打印字符
/*
构建用户进程初始信息上下文,并跳转到filename_处指向
*/
static void start_process(void* filename_){
    void* function = filename_;//暂时用函数代替可执行文件的入口点
    task_struct* cur = get_running_thread_pcb(); //获取当前pcb，来设置中断返回后的新进程的寄存器值
    cur->self_kstack +=sizeof(thread_stack);
    intr_stack* proc_stack = (intr_stack*)cur->self_kstack;
    //
    proc_stack->edi = proc_stack->esi=proc_stack->ebp=proc_stack->esp=0;
    //
    proc_stack->ebx=proc_stack->edx=proc_stack->ecx=proc_stack->eax = 0;
    //
    proc_stack->gs = 0;
    //
    proc_stack->ds = proc_stack->es=proc_stack->fs = SELECTOR_U_DATA;
    //
    proc_stack->old_eip = function;
    proc_stack->old_cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0|EFLAGS_MBS|EFLAGS_IF_1);
    proc_stack->old_esp = (void*)((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR)+PG_SIZE);//3环进程的栈指向0xc0000000
    assert(proc_stack->old_esp!=NULL);
    proc_stack->old_ss = SELECTOR_U_DATA; //使用数据段的选择子,虽然栈是由高到低,但是栈的还是向上扩展的。
    print("The execution process is about to start \n");
    asm volatile ("movl %0,%%esp;jmp intr_exit": : "g"(proc_stack) : "memory");
}
/*
功能:修改内核线程和用户进程的CR3
*/
void page_dir_activate(task_struct *p_thread){
    uint32_t pagedir_phy_addr = 0x100000;
    if(p_thread->pgdir!=NULL){//用户进程有自己的页目录表
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
    }
    /*更新cr3*/
//  asm volatile("movl %0,%%cr3"::"r"(pagedir_phy_addr));
    asm volatile ("movl %0,%%cr3" : : "r"(pagedir_phy_addr) : "memory");
}
/*
功能:激活页表,更新tss中的esp0为进程的特权级0栈
*/
void process_activate(task_struct* p_thread){
    assert(p_thread!=NULL);
    //
    page_dir_activate(p_thread);
    if(p_thread->pgdir){
        //更新进程的esp0
        update_tss_esp(p_thread);
    }

}
/*
功能：创建用户进程所需的页目录表,并且复制内核空间的PDE,实现高1G共同映射内核
返回:成功返回页目录的虚拟地址。失败返回NULL
*/
uint32_t* create_page_dir(void){
    //在内核空间分配一个虚拟地址,避免用户去访问页表

    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    if(page_dir_vaddr==NULL){
        print("create_page_dir:get_kernel_pages failed\n");
        return;
    }
    //复制页表 从第768项开始复制到1023项,一项4个字节 (1023-768+1)*4 = 1024字节
    //内核页目录的起始地址为0xfffff000
    memcpy((page_dir_vaddr+768),(uint32_t*)(0xfffff000+768*4),1024);
    //修改页目录表的最后一项,在用户进程中使其指向用户页目录的物理起始地址,内核中指向的为内核页目录的物理起始地址,
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
    page_dir_vaddr[1023] = new_page_dir_phy_addr|PG_US_U|PG_RW_W|PG_P_1;
    return page_dir_vaddr;
}
/*
功能:创建用户进程虚拟地址位图,用于管理用户进程堆
*/
static void create_user_vaddr_bitmap(task_struct* user_prog){
    
    //初始化用户虚拟内存池的起始地址
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    //计算位图所需要的占用的内存空间大小以页为单位。
    uint32_t bitmap_byte_cnt = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP(bitmap_byte_cnt,PG_SIZE);
    //在内核空间申请位图页
    user_prog->userprog_vaddr.bitmap_vaddr.bits = get_kernel_pages(bitmap_pg_cnt);
    assert(user_prog->userprog_vaddr.bitmap_vaddr.bits!=NULL);
    user_prog->userprog_vaddr.bitmap_vaddr.bitmap_byte_len = bitmap_byte_cnt;
    dbg_printf("\nnew user process  bitmap info:\n");
    dbg_printf("    userprog_vaddr.bitmap_vaddr.bits:0x%x\n",user_prog->userprog_vaddr.bitmap_vaddr.bits);
    dbg_printf("    user_prog->userprog_vaddr.bitmap_vaddr.bitmap_byte_len:0x%x\n\n",bitmap_byte_cnt);
    dbg_printf("    user bitmap page count:%d\n",bitmap_pg_cnt);

    bitmap_init(&user_prog->userprog_vaddr.bitmap_vaddr);

}
/*
功能:创建用户进程
参数:filename,用户进程起始地址,进程名称
*/
void process_execute(void* filename,char* name){

    task_struct* thread = get_kernel_pages(1);

    print("user process peb:");
    put_int32((uint32_t)thread);
    print("\n");

    init_thread(thread,name,default_prio);
    //为用户进程虚拟地址创建位图  
    create_user_vaddr_bitmap(thread);
    
    thread_create(thread,start_process,filename);

    thread->pgdir = create_page_dir();

    block_desk_init(thread->u_block_desc); //初始化内存块数组
    


    //添加进线程链表
    enum intr_status old_status = intr_disable();
    assert(!list_elem_find(&thread_ready_list,&thread->general_tag));
    assert(!list_elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    list_append(&thread_ready_list,&thread->general_tag);
    intr_set_status(old_status);
    assert(thread->stack_magic==STACK_MAGIC);
}

