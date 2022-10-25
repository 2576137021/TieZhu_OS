#include "wait_exit.h"
#include "../thread/thread.h"
#include "../lib/kernel/memory.h"
#include "../fs/fs.h"
#include "../lib/stdio.h"
#include "../lib/kernel/stdio-kernel.h"
/**
 * @brief 
 * 释放用户进程资源
 * 包括以下资源
 * 页表的物理页
 * 虚拟内存池的物理页
 * 关闭打开的文件
 */
static void release_prog_resource(task_struct* release_thread){
    uint32_t* pgdir_vaddr = release_thread->pgdir;
    uint16_t user_pde_nr = 768,pde_idx = 0;
    uint32_t pde = 0;
    uint32_t* v_pde_ptr = NULL;

    uint16_t user_pte_nr = 1024,pte_idx = 0;
    uint32_t pte = 0;
    uint32_t* v_pte_ptr = NULL;

    uint32_t* first_pte_vaddr_in_pde = NULL;
    uint32_t pg_phy_addr = 0;

    //回收页表中用户空间的页框
    while(pde_idx < user_pde_nr){
        v_pde_ptr = pgdir_vaddr +pde_idx;
        pde = *v_pde_ptr;
        if(pde & 1){
            first_pte_vaddr_in_pde = get_pte_ptr(pde_idx * 0x400000);
            pte_idx = 0;
            while(pte_idx <user_pte_nr){
                v_pte_ptr = first_pte_vaddr_in_pde + pte_idx;
                pte = *v_pte_ptr;
                if(pte & 1){
                    
                    pg_phy_addr = pte& 0xfffff000;
                    free_a_phy_page(pg_phy_addr);
                }
                pte_idx++;
            }
            pg_phy_addr = pde & 0xfffff000;
            free_a_phy_page(pg_phy_addr);
        }
        pde_idx++;
    }
    //回收用户虚拟地址池所占的物理内存
    uint32_t bitmap_pg_cnt = release_thread->userprog_vaddr.bitmap_vaddr.bitmap_byte_len /PG_SIZE;
    uint8_t* user_vaddr_pool_bitmap = release_thread->userprog_vaddr.bitmap_vaddr.bits;
    mfree_page(PF_KERNEL,user_vaddr_pool_bitmap,bitmap_pg_cnt);

    //关闭进程打开的文件
    uint8_t fd_idx = 3;
    while(fd_idx < MAX_FILES_OPEN_PER_PROC){
        if(release_thread->fd_table[fd_idx] != -1){
            //书上在这里添加了管道的处理，我感觉没必要毕竟已经在sys_close中处理过了
            sys_close(fd_idx);
        }
        fd_idx++;
    }
}
/**
 * @brief 
 * list回调
 * 查找pelem的pid是否是ppid
 * 成功返回true,失败返回false
 */
static bool find_child(list_elem* pelem,int32_t ppid){
    task_struct* pthread = struct_entry(pelem,task_struct,all_list_tag);
    if(ppid ==pthread->pid)return true;
    return false;
}
/**
 * @brief
 * list回调
 * 查找父进程pid为ppid并且状态为挂起的任务
 */
static bool find_hanging_child(list_elem* pelem,int32_t ppid){
    task_struct* pthread = struct_entry(pelem,task_struct,all_list_tag);
    if(pthread->parent_pid == ppid && pthread->status ==TASK_HANGING){
        return true;
    }
    return false;
}
/**
 * @brief 
 * list回调
 * 所有父进程为pid的全部过继给init任务
 */
static bool init_adopt_a_child(list_elem* pelem,int32_t pid){
    task_struct* pthread = struct_entry(pelem,task_struct,all_list_tag);
    if(pthread->parent_pid == pid){
        pthread->parent_pid = 1;
    }
    return false;
}
/**
 * @brief 
 * 子进程结束时调用,status退出状态
 */
void sys_exit(int32_t status){
    task_struct* child_thread = get_running_thread_pcb();
    child_thread->exit_status = status;
    if(child_thread->parent_pid == -1){
        printf("current task no parent task\n");
    }
    // 1.将当前进程的所有子进程过继给init
    list_traversal(&thread_all_list,(functionc*)init_adopt_a_child,(void*)child_thread->pid);
    // 2.回收进程child_thread的资源
    release_prog_resource(child_thread);
    // 3.唤醒正在等待的父进程
    task_struct* parent_thread = pid2thread(child_thread->parent_pid);
    if(parent_thread!=NULL && (parent_thread->status ==TASK_WAITING)){
        thread_unlock(parent_thread);
    }
    // 4.挂起自身,等待父进程回收pcb
    thread_block(TASK_HANGING);
}






/**
 * @brief 
 * 等待子进程调用exit,将子进程的退出状态保存到status中,status可以为NULL
 * 成功返回子进程的pid，失败返回-1
 */
pid_t sys_wait(int32_t* status){
    task_struct* parnet_thread = get_running_thread_pcb();
    while(true){
        // 1.子进程调用exit会将自身挂起,所以直接查找挂起的任务
        list_elem* child_elem = list_traversal(&thread_all_list,(functionc*)find_hanging_child,(void*)parnet_thread->pid);
        if(child_elem!= NULL){
            task_struct* child_thread = struct_entry(child_elem,task_struct,all_list_tag);
            //保存子进程退出状态到参数
            if(status!=NULL){
                *status = child_thread->exit_status;
            }
            uint16_t child_pid = child_thread->pid;
        // 2.回收进程剩余资源
            thread_exit(child_thread,false);
            //dbg_printf("child thread exit,pid %d\n",child_pid);
            return child_pid;
        }
        // 3.检查是否存在还在执行的子进程
        child_elem = list_traversal(&thread_all_list,(functionc*)find_child,(void*)parnet_thread->pid);
        //即不存在子进程，那么wait返回失败
        if(child_elem == NULL)return -1;
        thread_block(TASK_WAITING);
    }
}

