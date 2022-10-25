#ifndef _FS_DIR_H
#define _FS_DIR_H
#include "../lib/kernel/stdint.h"
#include "fs.h"


extern dir root_dir;
/**
 * @brief 
 * 将目录项 p_de写入父目录 parent_dir中,同步内存磁盘目录项数据到磁盘
 * io_buf::调用者提供的缓冲区,至少一个扇区大小
 * 此操作会改变 parent_dir 的inode属性值
 */
bool sync_dir_entry(dir* parent_dir,dir_entry* p_de,\
void* io_buf);
/**
 * @brief 
 * 在内存中初始化目录项p_de
 */
void create_dir_entry(char* filename,uint32_t inode_no,uint8_t file_type,\
dir_entry* p_de);
/**
 * @brief 
 * 关闭目录
 */
void dir_close(dir* dir);
/**
 * @brief 
 * 在part分区pdir目录搜索名为name的目录或文件
 * 成功返回true，目录项存入dir_e
 * 失败返回false
 */
bool search_dir_entry(partition* part,dir* pdir,const char* name,dir_entry* dir_e);
/**
 * @brief 
 * 在分区part上打开i节点为inode_no的目录并返回目录指针
 */
dir* dir_open(partition* part,uint32_t inode_no);
/**
 * @brief 
 * 打开根目录
 */
void open_root_dir(partition* part);
/**
 * @brief 
 * 回收pdir目录中inode编号为inode_no的目录项
 */
bool delete_dir_entry(partition* part,dir* pdir,uint32_t inode_no,void* io_buf);
/**
 * @brief 
 * 读取目录中的所有目录项(包括文件和目录)
 * 成功返回一个目录项，失败返回NULL
 */
dir_entry* dir_read(dir* dir);

#endif