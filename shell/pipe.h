#ifndef _SHELL_PIPE_H
#define _SHELL_PIPE_H
#include "../lib/kernel/stdint.h"
#define PIPE_FLAG 0xffff
/**
 * @brief 
 * 判断文件描述符 local_fd是否是管道
 * 成功返回true，失败返回false
 */
bool is_pipe(uint32_t local_fd);
/**
 * @brief 
 * 往管道中写入数据
 * 成功返回写入的字节数
 */
uint32_t pipe_write(int32_t fd, const void* buf, uint32_t count);
/**
 * @brief 
 * 从管道中读取数据
 * 成功返回读取的字节数，不存在失败
 */
uint32_t pipe_read(int32_t fd,void* buf,uint32_t count);
/**
 * @brief 
 * 创建管道
 * 成功返回TRUE,失败返回FALSE
 */
bool sys_pipe(int32_t pipefd[2]);
/**
 * @brief 
 * 重定向文件描述符
 * old_local_fd指向新的new_loca_fd
 */
void sys_fd_redirect(uint32_t old_local_fd,uint32_t new_local_fd);
#endif
