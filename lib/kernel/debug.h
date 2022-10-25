#ifndef _LIB_KERNEL_DEBUG
#define _LIB_KERNEL_DEBUG
#include "stdint.h"
void panic_spin(char* fileName,int line, const char* functionName, char*condition);
/*
... 表示接收可变参数
__VA_ARGS__ 表示所有的可变参数
*/
#define PANIC(...) panic_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)
/*定义 ASSERT */
#ifdef NDEBUG //如果存在 NDEBUG标志 则使断言失效
    #define ASSERT(COUNDITION){(void) 0}
#else
    #define ASSERT(COUNDITION){\
        if(COUNDITION){}\
        else {PANIC(#COUNDITION);}}//符号“#”让编译器将参数转换为字符串字面量.
#endif
void sleep_temp(uint32_t millisecond);
/**
 * @brief 
 * 检查mem_addr是否为NULL
 */
void mm_is_null(void* mem_addr);
#define assert ASSERT
#endif