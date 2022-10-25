#ifndef _LIB_USE_SYSCALL_H
#define _LIB_USE_SYSCALL_H
#include "../kernel/stdint.h"
#include "../../fs/fs.h"
enum SYSCALL_NR{
    SYS_GETPID,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE,
    SYS_FORK,
    SYS_READ,
    SYS_PUTCHAR,
    SYS_CLEAR,
    SYS_GETCWD,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_LSEEK,
    SYS_UNLINK,
    SYS_MKDIR,
    SYS_OPENDIR,
    SYS_CLOSEDIR,
    SYS_CHDIR,
    SYS_RMDIR,
    SYS_READDIR,
    SYS_REWINDDIR,
    SYS_STAT,
    SYS_PS,
    SYS_EXECV,
    SYS_WAIT,
    SYS_EXIT,
    SYS_GETCURSOR,
    SYS_PIPE,
    SYS_REDIRECT,
    SYS_HELP
};
/*返回当前任务pid*/
uint32_t getpid(void);
/*控制台打印输出*/
uint32_t write(int32_t fd,const void* buf,uint32_t count);
/*在当前进程堆中申请一块内存*/
void* malloc(uint32_t size);
/*释放有malloc申请的内存*/
void free(void* ptr);
/**
 * @brief 
 * 克隆用户进程
 * 成功父进程返回子进程pid,子进程返回0,失败返回-1,
 */
pid_t fork(void);
/**
 * @brief 
 * 从文件描述符fd中读取count个字节到buf,支持标准输入流
 * 成功返回读取的字节数，失败返回-1
 */
int32_t read(int32_t fd,void* buf,uint32_t count);
/**
 * @brief 
 * 清除屏幕内容
 */
void clear(void);
/**
 * @brief 
 * 输出一个字符
 */
void putchar(char char_asci);
/**
 * @brief 
 * 把当前工作目录绝对路径写入到buf,size是buf的大小。
 * 当buf为NULL时,由操作系统分配存储工作路径的空间。
 * 成功,返回字符串返回地址。失败返回NULL
 */
char* getcwd(char* buf,uint32_t size);
/*
 * @brief 
 * 功能:
 *  打开或创建文件
 * 参数:
 *  pathname:文件全路径
 *  flags:操作标识
 * 成功返回 pcb filetable 文件描述符
 * 失败返回 -1
 */
int32_t open(char* pathname,uint8_t flag);
/**
 * @brief 
 * 关闭文件
 */
bool close(int32_t fd_idx);
/**
 * @brief 
 * 设置文件指针位置
 * 成功返回新的文件指针偏移量，失败返回-1
 */
int32_t lseek(int32_t fd,int32_t offset,uint8_t whence);

/**
 * @brief 
 * 删除文件(不能删除目录)
 * 成功返回true，失败返回false
 */
int32_t unlink(const char* pathname);

/**
 * @brief 
 * 创建目录 pathname
 * 成功返回 true,失败返回false
 */
bool mkdir(const char* pathname);

/**
 * @brief 
 * 打开目录
 * 成功返回目录指针，失败返回NULL 
 */
dir* opendir(const char* name);

/**
 * @brief 
 * 关闭目录
 * 成功返回true,失败返回false
 */
bool closedir(dir* dir);
/**
 * @brief 
 * 删除空目录
 * 成功时返回true，失败时返回false
 */
bool rmdir(const char* pathname);

/**
 * @brief 
 * 读取目录dir的一个目录项,更新dir_pos的位置
 * 成功返回其目录项地址
 * 失败返回NUL
 */
dir_entry* readdir(dir* dir);
/**
 * @brief 
 * 把目录dir的dir_pos置零
 */
void rewinddir(dir* dir);

/**
 * @brief 
 * 在buf中填充文件结构相关信息。
 * 成功返回true.失败返回false
 */
bool stat(const char* path,struct _stat* buf);

/**
 * @brief 
 * 更改当前工作目录为path(绝对路径)
 * 成功返回true,失败返回false
 */
bool chdir(const char* path);
void ps(void);
/**
 * @brief 
 * 运行磁盘中path路径的程序,argv为参数字符串指针
 */
int32_t execv(const char* path, const char* argv[]);
/**
 * @brief 
 * 子进程结束时调用,status退出状态
 */
void exit(int32_t status);
/**
 * @brief 
 * 等待子进程调用exit,将子进程的退出状态保存到status中,status可以为NULL
 * 成功返回子进程的pid，失败返回-1
 */
pid_t wait(int32_t* status);
/*获取光标位置*/
int32_t cursor(int32_t* cur_pos);
/**
 * @brief 
 * 创建管道
 */
bool pipe(int32_t pipefd[2]);
/**
 * @brief 
 * 文件描述符重定向
 */
void fd_redirect(uint32_t old_local_fd,uint32_t new_local_fd);/**
 * @brief 
 * 显示shell支持的命令
 */
void help(void);
#endif
