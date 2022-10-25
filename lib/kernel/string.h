#ifndef __LIB_KERNEL_STRING_H
#define __LIB_KERNEL_STRING_H
#include"stdint.h"
/*将dst_地址处的size个字节的值设置为value*/
void memset(void* dst_,uint8_t value,uint32_t size);
/*将src_处的size个字节复制到dst_地址处*/
void memcpy(void* dst_,void* src_,uint32_t size);
/*逐字节比较buf1和buf2中的二进制大小,若buf1>buf2,返回1,若buf1=buf2,返回0,若buf1<buf2,返回-1*/
int32_t memcmp(void* buf1,void* buf2,uint32_t size);
/*获取字符串的长度,不包括结尾0*/
uint32_t strlen(const char* str);
/*从_Source复制整个字符串到_Destination,复制成功_Destination,返回,检查失败返回NULL。*/
char* strcpy(char* _Destination,char* _Source);
/*字符串比较,如果str1>str2则返回1,如果str1=str2,返回0,str1<str2,返回-1.不会进行长度检查可能会产生越界*/
int8_t strcmp(const char* str1,const char* str2);
/*从后往前查找字符串str中第一次出现字符ch的地址,查找失败返回NULL*/
char* strrchar(const char* str,char ch);
/*将src_拼接到dst_的后面,返回dst_*/
char* strcat(char*dst_,char* src_);
/*在字符串str中查找ch出现的次数*/
uint32_t strchrs(char* str,uint8_t ch);
/*从前往后查找字符串中第一次出现字符ch的地址，查找失败返回NULL*/
char* strchar(const char* str,char ch);
#endif