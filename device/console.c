#include "console.h"
#include"../thread/sync.h"
#include"../lib/kernel/print.h"
static lock consol_lock;//控制台锁
/*
功能:初始化控制台
*/
void console_init(void){
    lock_init(&consol_lock);
    print("console init success\n");
}
/*
功能:获取控制台
*/
static void console_acquire(void){
    lock_acquire(&consol_lock);
}
/*
功能:释放控制台
*/
static void console_release(void){
    lock_release(&consol_lock);
}
/*
功能:在控制台输出字符
*/
void console_put_char(uint8_t char_ascll){
    console_acquire();
    put_char(char_ascll);
    console_release();
}
/*
功能:在控制台输出字符串
*/
void console_put_str(char* str){
    console_acquire();
    print(str);
    console_release();
}
/*
功能:在控制台输出无符号十六进制整数
*/
void console_put_uint32(uint32_t number){
    console_acquire();
    put_int32(number);
    console_release();
}

