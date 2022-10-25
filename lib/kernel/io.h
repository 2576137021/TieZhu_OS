#ifndef _LIB_IO_H
#define _LIB_IO_H
#include "stdint.h"

//向端口写入一个字节
static inline void outb(uint16_t prot, uint8_t data){
    asm volatile("out %b0,%w1"::"a"(data),"Nd"(prot));
}
//将addr处的word_cnt个字写入端口port
static inline void outsw(uint16_t prot,const void* addr,uint32_t word_cnt){
    asm volatile("cld; rep outsw":"+S"(addr),"+c"(word_cnt):"d"(prot));
}
//从端口port读取一个字节返回
static inline uint8_t inb(uint16_t prot){
    uint8_t data;
    asm volatile("inb %w1,%b0":"=a"(data):"Nd"(prot));
    return data;
}

//从端口prot读取word_cnt个字写入到addr
static inline void insw(uint16_t prot,void* addr,uint32_t word_cnt){
    //insw 从prot处读取16位数据写入到es:edi指向的内存
    asm volatile("cld;rep insw":"+D"(addr),"+c"(word_cnt):"d"(prot):"memory");
}
#endif
