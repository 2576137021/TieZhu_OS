#ifndef _SHELL_SHELL_H
#define _SHELL_SHELL_H
#include "../lib/kernel/stdint.h"
#include "../fs/fs.h"
/*shell*/
void shell(void);
/*输出提示符*/
void print_prompt(void);
//解析完成的路径
extern char final_path[MAX_PATH_LEN];
#endif