#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H
#include"../lib/kernel/stdint.h"
/*
功能:在控制台输出无符号十六进制整数
*/
void console_put_uint32(uint32_t number);
/*
功能:在控制台输出字符串
*/
void console_put_str(char* str);
/*
功能:在控制台输出字符
*/
void console_put_char(uint8_t char_ascll);
/*
功能:初始化控制台
*/
void console_init(void);
#endif
