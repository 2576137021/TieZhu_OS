#include "syscall.h"
#include "../kernel/stdint.h"
/*
syscall流程
用户进程调用_syscallx,填充eax,进入kenel.s,根据eax值,跳转到syscall_table数组保存的方法地址。
*/

//无参syscall
#define _syscall0(NUMBER) ({\
    uint32_t retval;\
    asm volatile ("int $0x80" : "=a" (retval) : "a" (NUMBER) : "memory");\
    retval;})//大括号中的最后一个语句的值会被当初大括号代码块的返回值,要添加";"号


//1-3个参数的syscall,分别使用寄存器ebx,ecx,edx,eax传递功能号
#define _syscall1(NUMBER,ARG1) ({\
    uint32_t retval;\
    asm volatile ("int $0x80" : "=a" (retval) : "a" (NUMBER),"b" (ARG1): "memory");\
    retval;})
#define _syscall2(NUMBER,ARG1,ARG2) ({\
    uint32_t retval;\
    asm volatile ("int $0x80" : "=a" (retval) : "a" (NUMBER),"b" (ARG1),"c" (ARG2): "memory");\
    retval;})
#define _syscall3(NUMBER,ARG1,ARG2,ARG3) ({\
    uint32_t retval;\
    asm volatile ("int $0x80" : "=a" (retval) : "a" (NUMBER),"b" (ARG1),"c" (ARG2),"d" (ARG3): "memory");\
    retval;})
/*返回当前任务pid*/
uint32_t getpid(void){
    return _syscall0(SYS_GETPID);
}
/*控制台打印输出*/
uint32_t write(int32_t fd,const void* buf,uint32_t count){
    return _syscall3(SYS_WRITE,fd,buf,count);
}
/*在当前进程堆中申请一块内存*/
void* malloc(uint32_t size){
    return (void*)_syscall1(SYS_MALLOC,size);
}
/*释放有malloc申请的内存*/
void free(void* ptr){
     _syscall1(SYS_FREE,ptr);
}
/**
 * @brief 
 * 克隆用户进程
 * 成功父进程返回子进程pid,子进程返回0,失败返回-1,
 */
pid_t fork(void){
    _syscall0(SYS_FORK);
}
/**
 * @brief 
 * 从文件描述符fd中读取count个字节到buf,支持标准输入流
 * 成功返回读取的字节数，失败返回-1
 */
int32_t read(int32_t fd,void* buf,uint32_t count){
    _syscall3(SYS_READ,fd,buf,count);
}
/**
 * @brief 
 * 输出一个字符
 */
void putchar(char char_asci){
    _syscall1(SYS_PUTCHAR,char_asci);
}
/**
 * @brief 
 * 清除屏幕内容
 */
void clear(void){
    _syscall0(SYS_CLEAR);
}

char* getcwd(char* buf,uint32_t size)
{
    return (char*)_syscall2(SYS_GETCWD,buf,size);
}

int32_t open(char* pathname,uint8_t flag)
{
    return _syscall2(SYS_OPEN,pathname,flag);
}

bool close(int32_t fd_idx)
{
    return _syscall1(SYS_CLOSE,fd_idx);
}

int32_t lseek(int32_t fd,int32_t offset,uint8_t whence)
{
    return _syscall3(SYS_LSEEK,fd,offset,whence);
}

int32_t unlink(const char* pathname)
{
    return _syscall1(SYS_UNLINK,pathname);
}

bool mkdir(const char* pathname)
{
    return _syscall1(SYS_MKDIR,pathname);
}
dir* opendir(const char* name)
{
    return  (dir*)_syscall1(SYS_OPENDIR,name);
}

bool closedir(dir* dir)
{
    return _syscall1(SYS_CLOSEDIR,dir);
}

bool rmdir(const char* pathname)
{
    return _syscall1(SYS_RMDIR,pathname);
}

dir_entry* readdir(dir* dir)
{
    return (dir_entry*)_syscall1(SYS_READDIR,dir);
}

void rewinddir(dir* dir)
{
    _syscall1(SYS_REWINDDIR,dir);
}

bool stat(const char* path,struct _stat* buf)
{
    return _syscall2(SYS_STAT,path,buf);
}

bool chdir(const char* path)
{
    return _syscall1(SYS_CHDIR,path);
}

void ps(void)
{
    _syscall0(SYS_PS);
}
int32_t execv(const char* path, const char* argv[]){
    return _syscall2(SYS_EXECV,path,argv);
}
/**
 * @brief 
 * 等待子进程调用exit,将子进程的退出状态保存到status中,status可以为NULL
 * 成功返回子进程的pid，失败返回-1
 */
pid_t wait(int32_t* status){
    return _syscall1(SYS_WAIT,status);
}
/**
 * @brief 
 * 子进程结束时调用,status退出状态
 */
void exit(int32_t status){
    _syscall1(SYS_EXIT,status);
}
/*获取光标位置*/
int32_t cursor(int32_t* cur_pos){
     _syscall1(SYS_GETCURSOR,cur_pos);
}
/**
 * @brief 
 * 创建管道
 */
bool pipe(int32_t pipefd[2]){
    return _syscall1(SYS_PIPE,pipefd);
}
/**
 * @brief 
 * 文件描述符重定向
 */
void fd_redirect(uint32_t old_local_fd,uint32_t new_local_fd){
    _syscall2(SYS_REDIRECT,old_local_fd,new_local_fd);
}
/**
 * @brief 
 * 显示shell支持的命令
 */
void help(void){
    _syscall0(SYS_HELP);
}