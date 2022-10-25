#include "fork.h"
#include "../thread/thread.h"
#include "../fs/file.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../lib/kernel/interrupt.h"
#include "process.h"
#include "../shell/pipe.h"
extern void intr_exit(void);
/**
 * @brief 
 * 将父进程的pcb和内核栈拷给子进程
 */
static int32_t copy_pcb_vaddrbitmap_stack0(task_struct* child_thread,\
task_struct* parent_thread){
    //复制pcb整个页，里面包含进程pcb信息和0级的栈
    memcpy(child_thread,parent_thread,PG_SIZE);

    //重新定义的部分
    child_thread->elapsed_ticks = 0;
    child_thread->status = TASK_READY;
    child_thread->ticks = child_thread->priority;
    child_thread->parent_pid = parent_thread->pid;
    child_thread->pid = fork_pid();
    child_thread->general_tag.next = child_thread->general_tag.prev = NULL;
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;

    //bug 忘了初始化子进程的内存块描述符
    block_desk_init(child_thread->u_block_desc);

    //复制父进程的虚拟地址池位图
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE /8,PG_SIZE);
    void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
    memcpy(vaddr_btmp,child_thread->userprog_vaddr.bitmap_vaddr.bits,bitmap_pg_cnt * PG_SIZE);
    child_thread->userprog_vaddr.bitmap_vaddr.bits = vaddr_btmp;

    //debug
    assert(strlen(child_thread->name)<14);
    strcat(child_thread->name,"_f");
    return 0;
}
/**
 * @brief 
 * 复制子进程的进程体(代码和数据)和用户栈
 */
static void copy_body_stack3(task_struct* child_thread,\
task_struct* parent_thread,void* buf_page){
    uint8_t* vaddr_btmp = parent_thread->userprog_vaddr.bitmap_vaddr.bits;
    uint32_t btmp_bytes_len =  parent_thread->userprog_vaddr.bitmap_vaddr.bitmap_byte_len;
    uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
    uint32_t idx_byte=0,idx_bit=0,prog_vaddr = 0;
    while(idx_byte<btmp_bytes_len)
    {
        if(vaddr_btmp[idx_byte])
        {
            idx_bit = 0;
            while(idx_bit<8)
            {
                if((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte])
                {   
                    //父进程被使用的虚拟地址页
                    prog_vaddr = (idx_byte * 8+ idx_bit) * PG_SIZE +vaddr_start;
                    memcpy(buf_page,(void*)prog_vaddr,PG_SIZE);
                    
                    //切换为子进程的cr3
                    page_dir_activate(child_thread);

                    get_a_page_without_opvaddrbitmap(PF_USER,prog_vaddr);
                    memcpy((void*)prog_vaddr,buf_page,PG_SIZE);
                    //恢复父进程cr3
                    page_dir_activate(parent_thread);
                }
                idx_bit++;
            }
        }
        idx_byte++;
    }
}

/**
 * @brief 
 * 为子进程构建thread_stack和修改返回值
 */
static int32_t build_child_stack(task_struct* child_thread){
    intr_stack* intr_0_stack = (intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(intr_stack));
    intr_0_stack->eax = 0;

    //这里需要重新看一下代码
    /*
        这里通过修改内核栈指针,指向中断栈-5的位置
        为什么是-5?
        因为在switch_to中,通过4个pop+一个ret切换任务。我们这里构建好ret时的返回位置为中断返回
        (返回后的子进程eip也就是父进程进入中断后保存的eip)
    */
    uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack -5;
    child_thread->self_kstack = ebp_ptr_in_thread_stack;

    uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack-1;
    *ret_addr_in_thread_stack = (uint32_t)intr_exit;

    
    return 0;
}
/**
 * @brief 
 * 更新inode打开数 
 */
static void update_inode_open_cnts(task_struct* thread){
    int32_t local_fd = 3,global_fd  = 0;
    while(local_fd < MAX_FILES_OPEN_PER_PROC){//此处和书中不一致
        global_fd = thread->fd_table[local_fd];
        if(global_fd != -1){
            dbg_printf("local_fd:%d,global_fd:%d,file_table base:0x%x\n",local_fd,global_fd,file_table);
            if(is_pipe(local_fd)){
                file_table[global_fd].fd_pos+=1;
            }else{
                file_table[global_fd].fd_inode->i_open_cnts+=1;
            }
            
        }
        local_fd++;
    }
}

/**
 * @brief 
 * 拷贝父进程本身所占资源给子进程
 */
static int32_t copy_process(task_struct* child_thread, task_struct* parent_thread){
    //中转缓冲区
    void* buf_page = get_kernel_pages(1);
    if(buf_page == NULL){
        return -1;
    }
    //复制pcb,内核栈信息,虚拟地址位图信息（大小为1页）
    if(copy_pcb_vaddrbitmap_stack0(child_thread,parent_thread) == -1){
        return -1;
    }
    //为子进程创建页表
    child_thread->pgdir = create_page_dir();
    if(child_thread->pgdir == NULL){
        return -1;
    }

    //复制父进程3环所有内存信息
    copy_body_stack3(child_thread,parent_thread,buf_page);

    //构建子进程thread_stack和修改返回值pid
    build_child_stack(child_thread);

    //更新文件inode的打开次数
    update_inode_open_cnts(child_thread);

    mfree_page(PF_KERNEL,buf_page,1);
    
    return 0;
}
/**
 * @brief 
 * 克隆用户进程
 * 成功父进程返回子进程pid,子进程返回0,失败返回-1,
 */
pid_t sys_fork(void){
    task_struct* parent_thread = get_running_thread_pcb();
    task_struct* child_thread = get_kernel_pages(1);
    //构建子进程pcb
    if(child_thread ==NULL){
        return -1;
    }
    //保证是在关中断,且是在3环进程中使用
    assert(INTR_OFF == get_intr_status() && (parent_thread->pgdir !=NULL));

    if(copy_process(child_thread,parent_thread) == -1)return -1;

    //添加子进程到就绪列表和所有线程链表
    assert(!list_elem_find(&thread_ready_list,&child_thread->general_tag));
    list_append(&thread_ready_list,&child_thread->general_tag);

    assert(!list_elem_find(&thread_all_list,&child_thread->all_list_tag));
    list_append(&thread_all_list,&child_thread->all_list_tag);

    //debug
    //dbg_printf("fork process info:\n");
    //dbg_printf("    pcb:0x%x\n",child_thread);
    //dbg_printf("    child_thread->priority:%d\n",child_thread->priority);

    return child_thread->pid;
}


    