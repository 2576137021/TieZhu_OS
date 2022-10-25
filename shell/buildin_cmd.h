#ifndef _SHELL_BUILDIN_CMD_H
#define _SHELL_BUILDIN_CMD_H
#include "../lib/kernel/stdint.h"
/**
 * @brief 
 * 解析路径(path)为绝对路径,final_path保存解析好的路径
 */
void make_clear_abs_path(char* path,char* final_path);
/**
 * @brief 
 * pwd命令的内部函数实现
 */
void buildin_pwd(int32_t argc,char** argv);

/**
 * @brief 
 * cd 命令
 * 成功返回新路径,失败返回NULL
 */
char* buildin_cd(int32_t argc ,char** argv);
/**
 * @brief 
 * ls 命令
 */
void buildin_ls(int32_t argc ,char** argv);
/**
 * @brief 
 * ps 命令
 */
void buildin_ps(int32_t argc,char** argv);
/**
 * @brief 
 * clear 命令
 */
void buildin_clear(int32_t argc,char** argv);

/**
 * @brief 
 * mkdir 命令
 * 成功返回true
 * 失败返回false
 */
int32_t buildin_mkdir(int32_t argc,char**argv);
/**
 * @brief 
 * rmdir 命令
 */
int32_t buildin_rmdir(int32_t argc, char** argv);

/**
 * @brief 
 * rm 命令
 */
int32_t buildin_rm(int32_t argc, char** argv);
#endif