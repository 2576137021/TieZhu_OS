#include "inode.h"
#include "super_block.h"
#include "fs.h"
#include "../device/ide.h"
#include "../thread/thread.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/stdio-kernel.h"

/**
 * @brief 
 * 计算inode所在的扇区和扇区内的偏移量
 */
void inode_locate(partition* part,uint32_t inode_no,inode_position* inode_pos){
    assert(inode_no<4096);
    uint32_t inode_table_lba = part->sb->inode_table_lba;
    uint32_t inode_size = sizeof(inode);
    /*基于inode_table_lba的inode偏移量*/
    //字节偏移
    uint32_t off_size = inode_no*inode_size;
    //整扇区偏移
    uint32_t off_sec = off_size/512;
    //扇区内字节偏移
    uint32_t off_size_in_sec = off_size%512;
    uint32_t lef_in_sec = 512-off_size_in_sec; //扇区剩余空间
    if(lef_in_sec<inode_size){ //用扇区剩余空间小于inode结构体的大小。那么inode必然跨越扇区.
        inode_pos->two_sec = true;
    }else{
        inode_pos->two_sec = false;
    }
    //注意！！！！！！！！！这里和书上不一致
    //已修改回来和书上一致，因为lba是以一个扇区为单位。我忘记了
    inode_pos->sec_lba = inode_table_lba + off_sec;
   
    inode_pos->off_size = off_size_in_sec;
}
/**
 * @brief 
 * 将inode写到分区part
 * io_buf 至少两个扇区大小
 */
void inode_sync(partition* part,inode* inode,void* io_buf){
    uint8_t inode_no = inode->i_no;
    inode_position inode_pos;
    inode_locate(part,inode_no,&inode_pos);



    assert(inode_pos.sec_lba>part->start_lba);

    struct _inode pure_inode;
    memcpy(&pure_inode,inode,sizeof(struct _inode));
    
    /*以下三个inode属性只在内存中起作用，存入硬盘时置默认值*/
    pure_inode.i_open_cnts = 0;
    pure_inode.write_deny =false;
    pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

    char* inode_buf  = (char*)io_buf;
    uint8_t sector_cnt = 0;
    if(inode_pos.two_sec)sector_cnt=2;
    else sector_cnt = 1;
    //如果inode跨越了两个扇区,存储前就要读出这两个扇区,修改后再存入两个扇区
    ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,sector_cnt);
    memcpy((inode_buf+inode_pos.off_size),&pure_inode,sizeof(struct _inode));
    ide_write(part->my_disk,inode_pos.sec_lba,inode_buf,sector_cnt);
}
/**
 * @brief 
 * 根据inode_no返回相应的inode
 */
inode* inode_open(partition* part,uint32_t inode_no){
    /*先尝试在已打开的inode链表中查找*/
    list_elem* elem = part->open_inodes.head.next;
    inode* inode_found;
    while (elem != &part->open_inodes.tail)
    {
        inode_found = (inode*)struct_entry(elem,inode,inode_tag);
        if(inode_found->i_no ==inode_no){
            inode_found->i_open_cnts++; //inode的打开次数增加.
            return inode_found;
        }
        elem = elem->next;
    }
    /*没有在已打开链表中找到,从磁盘中读取inode*/
    inode_position inode_pos;
    //获取inode在磁盘中的位置
    inode_locate(part,inode_no,&inode_pos);
    
    //修改当前进程的cr3为null,使sys_malloc在内核申请一块内存，以便inode链表与其他进程共享
    task_struct* cur = get_running_thread_pcb();
    uint32_t* cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    inode_found = (inode*)sys_malloc(sizeof(struct _inode));
    cur->pgdir = cur_pagedir_bak;

    /*读取磁盘中的inode*/
    char* inode_buf;//用于io操作的缓冲区
    if(inode_pos.two_sec){
        inode_buf = (char*)sys_malloc(512*2);
        ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,2);
    }else{
        inode_buf = (char*)sys_malloc(512);
        ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,1);
    }
    
    memcpy(inode_found,inode_buf+inode_pos.off_size,sizeof(struct _inode));
    inode_found->i_open_cnts = 1;
    /*将inode插入到链表中*/
    list_push(&part->open_inodes,&inode_found->inode_tag);

    sys_free(inode_buf);
    return inode_found;
}

/**
 * @brief 
 * 关闭inode或减少inode的打开数
 */
void inode_close(inode* inode){

    enum intr_status old_status = intr_disable();
    if(--inode->i_open_cnts ==0){
        //关闭inode
        list_remove(&inode->inode_tag);
        task_struct* cur = get_running_thread_pcb();
        uint32_t* cur_pgdir = cur->pgdir;
        //因为申请时是在内核堆空间中申请的，所以释放时也要保证是内核内存释放。
        cur->pgdir =NULL;
        sys_free(inode);
        cur->pgdir = cur_pgdir;
    }
    intr_set_status(old_status);
}
/**
 * @brief 
 * 初始化inode
 */
void inode_init(uint32_t inode_no,inode* new_inode){
    new_inode->i_no = inode_no;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;
    new_inode->i_bsize = 0;
    uint32_t i_no = 0;
    while (i_no<13)
    {
        new_inode->i_sectors[i_no] = 0;
        i_no++;
    }
    
}
/**
 * @brief 
 * 复制inode的所有数据块到buffer
 */
void copy_inode_isectors(disk* hd,inode* inode,uint32_t* buffer){
    uint8_t block_idx = 0;
    //先复制12个直接块
    while (block_idx<12)
    {
       buffer[block_idx] =  inode->i_sectors[block_idx];
       block_idx++;
    }
    //复制一个扇区的间接块
    if(inode->i_sectors[12]){
        ide_read(hd,inode->i_sectors[12],buffer+12,1);
    }

}
