#ifndef _LIB_KERNEL_PRINT_H
#define _LIB_KERNEL_PRINT_H
#include "stdint.h"
void put_char(uint8_t char_asci);
void print(char* printStr);
void put_int32(int pInt32);
//设置光标位置
void set_cursor(uint16_t cursot);
void show_fs(void);
void cls_screen(void);
/*获取光标位置*/
int32_t get_cursor(int32_t* cur_pos);
#endif
