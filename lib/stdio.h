#ifndef _LIB_STDIO_H
#define _LIB_STDIO_H
#include "./kernel/stdint.h"
typedef char* va_list;
//ap指向第一个固定参数 ap为va_list，v为format
#define va_start(ap,v) ap = (va_list)&v //这里取地址,用于在方法内取栈地址，通过第一个参数的栈地址，定位其他参数地址
//ap指向下一个参数,并返回其值
#define va_arg(ap,t) (*(t*)(ap+=4))
#define va_end(ap) ap = NULL
/*
功能:printf
返回:被打印的字符串长度
*/
uint32_t printf(const char* format,...);
/*
功能:根据格式化字符串,创建新的字符串
参数:str,新的字符串,format,格式化字符串,ap,已初始化的参数指针
返回：新字符串的长度
*/
uint32_t vsprintf(char* str,const char* format, va_list ap);
/*
功能:格式化字符串，写入到缓存区buf
返回:新字符串长度
*/
uint32_t sprintf(char* buf,const char* format,...);

#endif