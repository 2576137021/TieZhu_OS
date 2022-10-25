#include "../lib/kernel/stdint.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/debug.h"
/*将dst_地址处的size个字节的值设置为value*/
void memset(void* dst_,uint8_t value,uint32_t size){
    if(size==0)return;
    ASSERT(dst_ != NULL);
    uint8_t* Pdst_ = (uint8_t*)dst_;
    while (size--)
    {
        *Pdst_++ = value;
    }   
}
/*将src_处的size个字节复制到dst_地址处*/
void memcpy(void* dst_,void* src_,uint32_t size){
    if(size==0)return;
    ASSERT(dst_ != NULL);
    ASSERT(src_ != NULL);
    uint8_t* Pdst_ = (uint8_t*)dst_;
    uint8_t* Psrc_ = (uint8_t*)src_;
    while (size--)
    {
        *Pdst_++ = *Psrc_++;
    } 
}
/*逐字节比较buf1和buf2中的二进制大小,若buf1>buf2,返回1,若buf1=buf2,返回0,若buf1<buf2,返回-1*/
int32_t memcmp(void* buf1,void* buf2,uint32_t size){
    if(size==0)return;
    ASSERT(buf1 != NULL);
    ASSERT(buf2 != NULL);
    uint8_t* Pbuf1 = (uint8_t*)buf1;
    uint8_t* Pbuf2 = (uint8_t*)buf2;
    int32_t offst = 0;
    while (size--)
    {
       if (*Pbuf1+offst>*Pbuf2+offst)
       {
           return 1;
       }else if (*Pbuf1+offst<*Pbuf2+offst)
       {
           return -1;
       }
       offst++;
    }
    return 0; 
}
/*获取字符串的长度,不包括结尾0*/
uint32_t strlen(const char* str){
    assert(str!=NULL);
    uint32_t len = 0;
    while (*str++)
    {
        len++;
    }
    return len;  
}
/*从_Source复制整个字符串到_Destination,复制成功_Destination,返回,检查失败返回NULL。*/
char* strcpy(char* _Destination, char* _Source){
    assert(_Destination != NULL && _Source!=NULL);
    char* temp_Destination = _Destination;
    while (*_Source)
    {
        *_Destination++ = *_Source++;
    }
    *_Destination=0;//复制末尾的0;
    return temp_Destination;
}
/*字符串比较,如果str1>str2则返回1,如果str1=str2,返回0,str1<str2,返回-1.不会进行长度检查可能会产生越界*/
int8_t strcmp(const char* str1,const char* str2){
    assert(str1 != NULL && str2!=NULL);
    int32_t s1len = strlen(str1);
    int32_t s2len = strlen(str2);
    while (*str1 != 0)
    {  
       if(*str1>*str2)return 1;
       else if (*str1<*str2)return -1;
       str1++; //忘记++,导致死循环
       str2++;
       s1len--;
       s2len--;
    }
    if(s1len>s2len){
        return 1;
    }else if(s1len<s2len){
        return -1;
    }else{
        return 0;
    }
}
/*从后往前查找字符串str中第一次出现字符ch的地址,查找失败返回NULL*/
char* strrchar(const char* str,char ch){
    assert(str != NULL);
    uint32_t slen = strlen(str);
    str = str + slen - 1;
    while (slen--)
    {
        if (*(char*)str-- == ch)return (char*)++str;
    }
    return NULL;
}
/*从前往后查找字符串中第一次出现字符ch的地址，查找失败返回NULL*/
char* strchar(const char* str,char ch){
    assert(str != NULL);
    uint32_t slen = strlen(str);
    while (slen--)
    {
        if (*(char*)str++ == ch)return (char*)--str;
    }
    return NULL;
}
/*将src_拼接到dst_的后面,返回dst_*/
char* strcat(char*dst_,char* src_){
    assert(dst_!=NULL&&src_!=NULL);
    memcpy(dst_+strlen(dst_),src_,strlen(src_)+1);
    return dst_;
}
/*在字符串str中查找ch出现的次数*/
uint32_t strchrs(char* str,uint8_t ch){
    assert(str!=NULL);
    uint32_t count = 0;
    while (*str)
    {
        if(*str++==ch)count++;
    }
    return count;
}

