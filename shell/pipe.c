#include "pipe.h"
#include "../device/ioqueue.h"
#include "../fs/fs.h"
#include "../fs/file.h"
#include "../lib/kernel/debug.h"
#include "../lib/stdio.h"
#include "../lib/kernel/stdio-kernel.h"
//复用文件结构类型为管道类型
/*
typedef struct _file{
	    uint32_t fd_pos; //定义为管道的打开次数
	    uint32_t fd_flag;//专属于管道的flags(0xFFFF)
	    inode* fd_inode;//内核缓冲区
}file;
*/

/**
 * @brief 
 * 判断文件描述符 local_fd是否是管道
 * 成功返回true，失败返回false
 */
bool is_pipe(uint32_t local_fd){
    uint32_t global_fd = fd_idx2global_ftable_idx(local_fd);
    return file_table[global_fd].fd_flag ==PIPE_FLAG;
}
/**
 * @brief 
 * 创建管道(pipefd[0])
 * 成功返回TRUE,失败返回FALSE
 */
bool sys_pipe(int32_t pipefd[2]){

    // 1.获取全局文件描述符
    int32_t global_fd = get_free_slot_in_file_table();
    dbg_printf("pipe create global fd:%d\n",global_fd);
    file* fpipe =&file_table[global_fd];
    assert(global_fd!=-1);

    // 2.为管道分配buffer
    fpipe->fd_inode = get_kernel_pages(1);
    dbg_printf("pipe buffer:0x%x\n",fpipe->fd_inode);
    assert(fpipe->fd_inode!=NULL)

    // 3.初始化环形缓冲区
    ioqueue_init((ioqueue*)fpipe->fd_inode);

    // 4.初始化管道file属性
    fpipe->fd_flag = PIPE_FLAG;
    fpipe->fd_pos = 2;
    pipefd[0] = pcb_fd_install(global_fd);
    pipefd[1] = pcb_fd_install(global_fd);
    dbg_printf("pipe create local fd:%d\n",pipefd[0]);
    dbg_printf("pipe create local fd:%d\n",pipefd[1]);
    return true;

}
/**
 * @brief 
 * 从管道中读取数据
 * 成功返回读取的字节数，不存在失败
 */
uint32_t pipe_read(int32_t fd,void* buf,uint32_t count){
    char* buffer = buf;
    uint32_t bytes_read = 0;
    uint32_t global_fd = fd_idx2global_ftable_idx(fd);

    // 1.获取管道环形缓冲区
    ioqueue* ioq = (ioqueue *)file_table[global_fd].fd_inode;
    // 2.读出管道内的数据
    uint32_t ioq_len = ioq_length(ioq);
    uint32_t size;
    if(ioq_len>count){
        size = count;
    }else{
        size = ioq_len;
    }
    // 3.读取较小的数值,避免阻塞(在完成shell中使用管道后，回来理解)
    while(bytes_read<size){
        *buffer = ioq_getchar(ioq);
        bytes_read++;
        buffer++;
    }
    if(bytes_read==0)return -1;
    return bytes_read;
}
/**
 * @brief 
 * 往管道中写入数据
 * 成功返回写入的字节数
 */
uint32_t pipe_write(int32_t fd, const void* buf, uint32_t count){
    uint32_t bytes_write = 0;
    uint32_t global_fd = fd_idx2global_ftable_idx(fd);
    ioqueue* ioq = (ioqueue *)file_table[global_fd].fd_inode;

    /*选择较小的写入量，避免阻塞*/
    uint32_t ioq_left = buffer_size - ioq_length(ioq);
    uint32_t size;
    if(ioq_left>count){
        size = count;
    }else{
        size = ioq_left;
    }
   // dbg_printf("ioq_left:%d,size:%d,count:%d\n",ioq_left,size,count);
    const char* buffer = buf;
    while(bytes_write < size){
        ioq_putchar(ioq,*buffer);
        bytes_write++;
        buffer++;
    }
    return bytes_write;
}
/**
 * @brief 
 * 重定向文件描述符
 * old_local_fd指向新的new_loca_fd
 */
void sys_fd_redirect(uint32_t old_local_fd,uint32_t new_local_fd){
    task_struct* cur = get_running_thread_pcb();
    //重定向就是修改当前文件描述符存储的全局文件表下标
    // 0-2是预留给标准输入输出的
    if(new_local_fd<3){
        cur->fd_table[old_local_fd] = new_local_fd;
    }else{
        cur->fd_table[old_local_fd] = cur->fd_table[new_local_fd];
    }
}