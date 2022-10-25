#include "file.h"
#include "fs.h"
#include "../lib/kernel/stdio-kernel.h"
#include "super_block.h"
#include "dir.h"
/*全局文件表*/
file file_table[MAX_FILE_OPEN];

/**
 * @brief 
 * 从file_table中获取空闲位置
 * 成功返回数组下标,失败返回-1
 * file结构fd_inode属性==null 表示可用
 */
int32_t get_free_slot_in_file_table(void){
    uint32_t fd_idx = 3;//预留给标准输入输出
    while (fd_idx<MAX_FILE_OPEN)
    {
        if(file_table[fd_idx].fd_inode==NULL)return fd_idx;
        fd_idx++;
    }
    dbg_printf("exceed max open file! MAX_FILE_OPEN:%d\n",MAX_FILE_OPEN);   
}
/**
 * @brief 
 * 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中
 * 成功返回下标，失败返回-1
 */
int32_t pcb_fd_install(int32_t globa_fd_idx){
    task_struct * cur = get_running_thread_pcb();
    uint8_t local_fd_idx = 3;
    while (local_fd_idx< MAX_FILES_OPEN_PER_PROC)
    {
        if(cur->fd_table[local_fd_idx] == -1){
            cur->fd_table[local_fd_idx] = globa_fd_idx;
            return local_fd_idx;
        }
        local_fd_idx++;
    }
    dbg_printf("excedd max open files_per_proc\n");
    return -1;
}
/**
 * @brief 
 * 在分区part中inode位图中分配一个inode节点
 * 成功返回节点号，失败返回-1
 */
int32_t inode_bitmap_alloc(partition * part){
    int32_t bit_idx = bitMap_scan(&part->inode_bitmap,1);
    if(bit_idx!=-1){
        bitmap_set(&part->inode_bitmap,bit_idx,1);
    }
    return bit_idx;
}
/**
 * @brief 
 * 分配一个扇区,新分配的扇区将被初始化为0
 * 成功返回扇区lba地址
 * 失败返回-1
 */
int32_t block_bitmap_alloc(partition* part){

    void* io_buf = sys_malloc(SECTOR_SIZE);
    if(io_buf==NULL){
        return -1;
    }
    int32_t bit_idx = bitMap_scan(&part->block_bitmap,1);
    if(bit_idx==-1){
        return -1;
    }
    bitmap_set(&part->block_bitmap,bit_idx,1);
    //清空新申请的块
    ide_write(cur_part->my_disk,part->sb->data_start_lba+bit_idx,io_buf,1);
    sys_free(io_buf);
    return (part->sb->data_start_lba+bit_idx);
}
/**
 * @brief 
 * 同步内存中bitmap的第bit_idx位开始的512字节到硬盘
 * btmp:位图类型
 */
void bitmap_sync(partition* part,uint32_t bit_idx,uint8_t btmp){
    
    uint32_t off_sec = bit_idx/4096; //保存位图，相对于位图起始扇区的扇区偏移

    uint32_t off_size = off_sec*512; //被复制的位图字节偏移

    uint32_t sec_lba;
    uint8_t* bitmap_off;

    switch(btmp){
        case INODE_BITMAP:
        {
            sec_lba = part->sb->inode_bitmap_lba+off_sec;
            bitmap_off = part->inode_bitmap.bits+off_size;
            break;
        }
        case BLOCK_BITMAP:
        {
            sec_lba = part->sb->block_bitmap_lba+off_sec;
            bitmap_off = part->block_bitmap.bits+off_size;
            break;
        }
    }
    ide_write(part->my_disk,sec_lba,bitmap_off,1);

}
/**
 * @brief 
 * 创建一个新的inode(书中没有。包括位图的申请,inode的初始化，不包括同步)
 * 成功返回inode编号，失败返回 false
 */
int32_t inode_create(inode* new_inode){
    int32_t inode_no = inode_bitmap_alloc(cur_part);
    if(inode_no ==-1){
        
        dbg_printf("inode_create:inode_bitmap_alloc error\n");
        return false;
    }
    inode_init(inode_no,new_inode);
    return new_inode->i_no;
}
