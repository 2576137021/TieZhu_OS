#ifndef _BIN_FS_FILE_H
#define _BIN_FS_FILE_H
#include "../lib/kernel/stdint.h"

#include "inode.h"
#include "fs.h"
/*文件结构*/
typedef struct _file{
    uint32_t fd_pos; //文件操作的偏移地址，最小为0.最大为-1?
    uint32_t fd_flag;
    inode* fd_inode;
}file;
/*标准输入输出描述符*/
enum std_fd{
    stdin_no, //0 标准输入
    stdout_no, //1 标准输出
    stderr_no //2 标准错误
};
enum bitmap_type{
    INODE_BITMAP, //inode位图
    BLOCK_BITMAP //块位图
};
#define MAX_FILE_OPEN 32 //系统能打开的最大文件数
extern file file_table[MAX_FILE_OPEN];/*全局文件表*/
/**
 * @brief 
 * 分配一个扇区,新分配的扇区将被初始化为0
 * 成功返回扇区lba地址
 * 失败返回-1
 */
int32_t block_bitmap_alloc(partition* part);
/**
 * @brief 
 * 同步内存中bitmap的第bit_idx位开始的512字节到硬盘
 * btmp:位图类型
 */
void bitmap_sync(partition* part,uint32_t bit_idx,uint8_t btmp);
/**
 * @brief 
 * 在分区part中inode位图中分配一个inode节点
 * 成功返回节点号，失败返回-1
 */
int32_t inode_bitmap_alloc(partition * part);
/**
 * @brief 
 * 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中
 * 成功返回下标，失败返回-1
 */
int32_t pcb_fd_install(int32_t globa_fd_idx);
/**
 * @brief 
 * 从file_table中获取空闲未知
 * 成功返回数组下标,失败返回-1
 */
int32_t get_free_slot_in_file_table(void);
/**
 * @brief 
 * 创建一个新的inode(书中没有。包括位图的申请,inode的初始化，不包括同步)
 * 成功返回inode编号，失败返回 false
 */
int32_t inode_create(inode* new_inode);
#endif