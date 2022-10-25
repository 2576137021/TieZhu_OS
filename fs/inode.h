#ifndef _FS_INODE_H
#define _FS_INODE_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"
#include "../device/ide.h"
typedef struct _inode{
    uint32_t i_no; //inode编号,也就是在inode_table中的数组下标
    uint32_t i_bsize;//inode = 文件，i_size = 文件大小。inode=目录,i_size = 所有目录项大小之和。

    uint32_t i_open_cnts; //记录此文件被打开的次数
    bool write_deny; //写文件不能并行，进程写文件前检查此标志。

    uint32_t i_sectors[13];// 0-11为直接数据块,12为一级间接块的扇区地址。则能存储的总大小为:(512/4)+12=140块
    list_elem inode_tag; //用于加入已打开的文件链表，提示打开重复文件的速度

}inode;
/**
 * @brief 
 * 用于存储inode磁盘位置
 */
typedef struct _inode_position{
    bool two_sec;   //inode是否跨区//即inode有一部分在第一个扇区，剩余部分在第二个扇区
    uint32_t sec_lba;   //inode起始扇区lba地址
    uint8_t off_size;  //inode在扇区内的字节偏移
}inode_position; 




/**
 * @brief 
 * 关闭inode或减少inode的打开数
 */
void inode_close(inode* inode);
/**
 * @brief 
 * 初始化inode
 */
void inode_init(uint32_t inode_no,inode* new_inode);
/**
 * @brief 
 * 根据inode_no返回相应的inode
 */
inode* inode_open(partition* part,uint32_t inode_no);
/**
 * @brief 
 * 将inode写到分区part
 * io_buf 至少两个扇区大小
 */
void inode_sync(partition* part,inode* inode,void* io_buf);
/**
 * @brief 
 * 计算inode所在的扇区和扇区内的偏移量
 */

void inode_locate(partition* part,uint32_t inode_no,inode_position* inode_pos);
/**
 * @brief 
 * 复制inode的所有数据块到buffer
 */
void copy_inode_isectors(disk* hd,inode* inode,uint32_t* buffer);

#endif