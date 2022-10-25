#include "stdio.h"
#include "./kernel/stdint.h"
#include "./kernel/print.h"
#include "./kernel/string.h"
#include "./use/syscall.h"

/*
功能:将Int类型转换为对应的Ascll
参数:value,待转换的int整数,base,buf_ptr_addr保存转换后的字符buff二级指针,数字是16进制还是10进制
问题:buf_ptr_addr为什么要用二级指针
原因:方便修改buf指针指向的位置
*/
static void itoa(uint32_t value,char** buf_ptr_addr,uint8_t base){
    uint32_t m = value % base;//取余数
    uint32_t i = value / base;//去掉最低位
    if(i){
        itoa(i,buf_ptr_addr,base);
    }
    if(m<10){//0-9
        **buf_ptr_addr = '0'+m;
        (*buf_ptr_addr)++;
    }else if(m<0x10){//0xa-0xf
        **buf_ptr_addr = 'A'+m-10;
        (*buf_ptr_addr)++;
    }else{
        print("stdio itoa exception\n");
    }
}
/*
功能:根据格式化字符串,创建新的字符串
参数:str,新的字符串,format,格式化字符串,ap,已初始化的参数指针
返回：新字符串的长度
*/
uint32_t vsprintf(char* str,const char* format, va_list ap){
    char* buf_ptr = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;
    int32_t arg_value;
    while (index_char)//以0结尾的字符串
    {
        if(index_char!='%'){
           *buf_ptr++ = index_char;
           index_char = *(++index_ptr); //跳转到下一位
           continue;
        }
        index_char = *++index_ptr;//得到%之后的内容
        switch (index_char)
        {   
            case 'x':
            {
                arg_value = va_arg(ap,int32_t);
                itoa(arg_value,&buf_ptr,16);//itoa 中会修改bufer指针,以及赋值
                index_char = *(++index_ptr);//跳过'x'
                break;
            }
            case 'c':
            {
                arg_value = va_arg(ap,char);
                *buf_ptr++ = (char)arg_value;
                index_char = *(++index_ptr); //跳过
                break;
            }
            case 'd':
            {
                arg_value = va_arg(ap,int32_t);
                if (arg_value & 0x80000000) {
                    //最高位会在下面设为0
                    arg_value = ~arg_value + 1;//然后转为反码再+1
                    *buf_ptr++='-';
                }
                itoa(arg_value,&buf_ptr,10);
                index_char = *(++index_ptr); //跳过
                break;
            }
            case 's':
            {   
                arg_value = va_arg(ap,uint32_t);
                strcat(buf_ptr,(char*)arg_value);
                buf_ptr+=strlen((char*)arg_value);
                index_char = *(++index_ptr); //跳过
                break;
            }
            default:
            {   
                *(buf_ptr++) = index_char;
                index_char = *(++index_ptr); //跳过未定义的符号
                break;
            }
        }
    }
    return strlen(str); //返回新字符串的长度
}
/*
功能:printf
返回:被打印的字符串长度
*/
uint32_t printf(const char* format,...){
    va_list args;
    va_start(args,format);
    char buf[1024] = {0};
    vsprintf(buf,format,args);
    va_end(args);
    return write(1,buf,strlen(buf));
}
/*
功能:格式化字符串，写入到缓存区buf
返回:新字符串长度
*/
uint32_t sprintf(char* buf,const char* format,...){
    va_list args;
    va_start(args,format);
    uint32_t new_str_len = vsprintf(buf,format,args);
    va_end(args);
    return new_str_len;
}
