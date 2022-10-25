#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/string.h"
/*
打印文件名称、行号、方法名称、断言条件
*/
void panic_spin(char* fileName,int line, const char* functionName, char*condition){

    asm volatile ("cli");
    print("**********************************************************\n");
    print("Report a error,eror information:\n");
    print("FileName:");
    print(fileName);
    print("\nLine:");
    put_int32(line);
    print("\nFunctionName:");
    print((char*)functionName); 
    print("\nCondition:");
    print(condition);
    print("\n");
    print("**********************************************************\n");
    while(1);
}
void sleep_temp(uint32_t millisecond){
    while (millisecond--)
    {
        
    }
}
void mm_is_null(void* mem_addr){
    if (mem_addr ==NULL)
    {
        print("mm_is_null");
        while(1);
    }
    
}