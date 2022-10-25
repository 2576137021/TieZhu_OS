#include "tss.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/memory.h"
#include "../thread/thread.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/string.h"
#define GDT_BASE 0xc0000919
typedef struct _tss
{
    uint32_t backlink;//上一个TSS任务指针
    uint32_t* esp0; //进入0环前要切换的esp
    uint32_t ss0;
    uint32_t* esp1;
    uint32_t ss1;
    uint32_t* esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t (*eip) (void);
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint32_t trace;
    uint32_t io_base;
}TSS;
//创建静态tss结构体
static TSS tss;
/*
功能:更新tss中的esp0字段的值为pthread的0级栈
*/
void update_tss_esp(task_struct* pthread){
    tss.esp0 = (uint32_t*)((uint32_t)pthread+PG_SIZE);
}
/*
功能:创建GDT描述符
*/
static gdt_desc make_gdt_desc(uint32_t* desc_addr,uint32_t limit,uint8_t attr_low,uint8_t attr_high){
    uint32_t desc_base = (uint32_t)desc_addr;
    gdt_desc desc;
    desc.base_low_word = desc_base&0x0000ffff;
    desc.base_mid_byte = (desc_base&0x00ff0000)>>16;
    desc.base_high_byte = desc_base>>24;
    desc.limit_low_word=limit&0x0ffff;
    desc.limit_hight_attr_hight =((limit&0xf0000)>>16)+attr_high;
    desc.attr_low_byte=attr_low;
    return desc;
}
/*
功能:在gdt中创建tss描述符,3环代码段和数据段描述符,并重新加载gdt,
*/
void tss_init(void){
    memset(&tss,0,sizeof(TSS));
    tss.ss0=SELECTOR_K_STACK;
    //将tss描述符写入到GDT的第5个位置
    *(gdt_desc*)(GDT_BASE+32) = make_gdt_desc((uint32_t*)&tss,sizeof(TSS)-1,TSS_ATTR_LOW,
    TSS_ATTR_HIGH);
    //在GDT中添加3环代码段和数据段
    *(gdt_desc*)(GDT_BASE+40) = make_gdt_desc((uint32_t*)0,0xfffff,GDT_ATTR_LOW_CODE_DPL3,
    GDT_ATTR_HIGH);
    *(gdt_desc*)(GDT_BASE+48) = make_gdt_desc((uint32_t*)0,0xfffff,GDT_ATTR_LOW_DATA_DPL3,
    GDT_ATTR_HIGH);
    //加载TR,重新加载GDT
    uint64_t gdt_limit = ((uint64_t)GDT_BASE<<16)+55;//55 = 7个描述符*8byte-1
    asm volatile("lgdt %0"::"m" (gdt_limit));
    asm volatile("ltr %w0"::"r" (SELECTOR_TSS));
    print("tss_init success\n");

}
