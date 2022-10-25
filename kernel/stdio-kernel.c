#include "../lib/kernel/stdio-kernel.h"
#include "../lib/kernel/stdint.h"
#include "../lib/stdio.h"
#include "../device/console.h"
//内核输出函数(不调用sys_write,直接操作显存)
void dbg_printf(const char* format,...){
    return ;
    va_list args;
    va_start(args,format);
    char str[1024] = {0};
    vsprintf(str,format,args);
    va_end(args);
    console_put_str(str);
}