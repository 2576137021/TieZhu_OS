#ifndef _FS_FS_H
#define _FS_FS_H
#include "../lib/kernel/stdint.h"
#include "../device/ide.h"
#include "inode.h"
#define MAX_FILES_PER_PART 4096 //每个分区支持的最大文件数
#define SECTOR_SIZE 512        //一个扇区的字节大小
#define BITS_PER_SECTOR (SECTOR_SIZE*8)  //一个扇区的位数
#define BLOCK_SIZE SECTOR_SIZE  //块字节大小
#define MAX_PATH_LEN 512 //路径最大长度
#define MAX_FILE_NAME_LEN 16 //最大文件名长度

extern partition* cur_part; //当前所在分区
/*文件类型*/
enum file_type{
    FT_UNKNOW,      //文件不存在
    FT_REGULAR,     //普通文件
    FT_DIRECTORY    //目录
};
/*打开文件的选项*/
enum oflags{
    O_READ,    //只读
    O_WRITE,   //只写
    O_RW,           //可读可写
    O_CREAT = 4     //创建
};
/*文件读写偏移量*/
enum whence{
    SEEK_SET = 1,//文件开始处
    SEEK_CUR,//文件当前读写位置
    SEEK_END //文件最后一个字节+1
};
/*目录结构*/
typedef struct _dir{
    inode* inode;
    uint32_t dir_pos;//记录在目录内的偏移
    uint8_t dir_buf[512]; //目录的数据缓存
}dir;
/*目录项结构*/
typedef struct _dir_entry{
    char filename[MAX_FILE_NAME_LEN]; //普通文件名称或目录名称
    uint32_t i_no; //普通文件或目录文件对应的inode编号
    enum file_type f_type; //文件类型
}dir_entry;
/*文件属性结构*/
struct _stat{
    uint32_t st_ino;
    uint32_t st_size;
    enum file_type st_file_type;
};
/**
 * @brief 
 * 返回路径深度,/a/b/c的路径深度为3
 */
int32_t path_depth_cnt(const char* pathname);
/**
 * @brief 
 * 将最上层路径名解析出来
 * patname:完整路径
 * name_store：保存最上层路径
 * 路径为空返回NULL,成功返回未解析的路径
 */
char* path_parse(char* patname,char* name_store);
/**
 * @brief 
 * 初始化文件系统
 */
void filesys_init(void);
/**
 * @brief 
 * 功能:
 *  打开或创建文件
 *  pathname:文件全路径
 *  flags:操作标识
 * 成功返回 pcb filetable 文件描述符
 * 失败返回 -1
 */
int32_t sys_open(const char* pathname,uint8_t flags);
/**
 * @brief 
 * 关闭文件
 */
bool sys_close(int32_t fd_idx);
/**
 * @brief 
 * 输出文件系统属性值,eip
 */
void show_fs(void);
/**
 * @brief 
 * 将buf中连续的count字节写入到文件描述符为fd的文件
 * 默认情况不覆盖源文件,数据添加到文件末尾
 * 成功返回写入的字节数，失败返回-1
 */
int32_t sys_write(int32_t fd,const void* buf,uint32_t count);
/**
 * @brief 
 * 从文件描述符fd中读取count个字节到buf
 * 成功返回读取的字节数，失败返回-1
 */
int32_t sys_read(int32_t fd,void* buf,uint32_t count);
/**
 * @brief 
 * 设置文件指针位置
 * 成功返回新的文件指针偏移量，失败返回-1
 */
int32_t sys_lseek(int32_t fd,int32_t offset,uint8_t whence);
/**
 * @brief 
 * 删除文件(不能删除目录)
 * 成功返回true，失败返回false
 */
int32_t sys_unlink(const char* pathname);
/**
 * @brief 
 * 创建目录 pathname
 * 成功返回 true,失败返回false
 */
bool sys_mkdir(const char* pathname);
/**
 * @brief 
 * 关闭目录
 * 成功返回true,失败返回false
 */
bool sys_closedir(dir* dir);
/**
 * @brief 
 * 打开目录
 * 成功返回目录指针，失败返回NULL 
 */
dir* sys_opendir(const char* name);
/**
 * @brief 
 * 读取目录dir的一个目录项,更新dir_pos的位置
 * 成功返回其目录项地址
 * 失败返回NUL
 */
dir_entry* sys_readdir(dir* dir);
/**
 * @brief 
 * 把目录dir的dir_pos置零
 */
void sys_rewinddir(dir* dir);
/**
 * @brief 
 * 删除空目录
 * 成功时返回true，失败时返回false
 */
bool sys_rmdir(const char* pathname);
/**
 * @brief 
 * 把当前工作目录绝对路径写入到buf,size是buf的大小。
 * 当buf为NULL时,由操作系统分配存储工作路径的空间。
 * 成功,返回字符串返回地址。失败返回NULL
 */

char* sys_getcwd(char* buf,uint32_t size);
/**
 * @brief 
 * 更改当前工作目录为绝对路径path
 * 成功返回true,失败返回false
 */
bool sys_chdir(const char* path);
/**
 * @brief 
 * 在buf中填充文件结构相关信息。
 * 成功返回true.失败返回false
 */
bool sys_stat(const char* path,struct _stat* buf);
/**
 * @brief 
 * 文件描述符转换为全局文件表下标
 */
int32_t fd_idx2global_ftable_idx(uint32_t fd_idx);
/**
 * @brief 
 * 显示shell支持的命令
 */
void sys_help(void);
#endif