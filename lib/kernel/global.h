#ifndef _KERNEL_GLOBAL
#define _KERNEL_GLOBAL
#include "stdint.h"

#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TI_GDT 0
#define TI_LDT 1
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)

/*
定义3环和0环的IDT选择子
*/
#define IDT_P 1
#define IDT_DPL0 0
#define IDT_DPL3 3
#define IDT_S 0
#define IDT_TYPE 0xE
#define IDT_ATTRIBUTE (IDT_P<<7)|(IDT_DPL0<<5)|(IDT_S<<4)|(IDT_TYPE)
/*
GDT描述符属性
*/
#define DESC_G_4K 1
#define DESC_D_32 1
#define DESC_L  0 //hight offset:20,is 64 ?
#define DESC_AVL    0
#define DESC_P  1
#define DESC_DPL_0 0
#define DESC_DPL_1 1
#define DESC_DPL_2 2
#define DESC_DPL_3 3
#define DESC_DB_32 1

#define DESC_S_CODE 1
#define DESC_S_DATA DESC_S_CODE
#define DESC_S_SYS 0
//TYPE 类型定义
#define DESC_TYPE_CODE 8 //可执行一致代码段
#define DESC_TYPE_DATA 2 //不可执行向上扩展数据段
#define DESC_TYPE_AVALLABLE_TSS 9 //空闲的TSS B=0
//定义内核代码、数据、栈段选择子
#define SELECTOR_K_CODE (1<<3)+(TI_GDT<<2)+RPL0
#define SELECTOR_K_DATA (2<<3)+(TI_GDT<<2)+RPL0
#define SELECTOR_K_STACK SELECTOR_K_DATA
#define SELECTOR_K_GS   (3<<3)+(TI_GDT<<2)+RPL0
//定义用户代码、数据、栈段选择子
#define SELECTOR_TSS (4<<3)+(TI_GDT<<2)+RPL0 //TSS选择子
#define SELECTOR_U_CODE (5<<3)+(TI_GDT<<2)+RPL3
#define SELECTOR_U_DATA (6<<3)+(TI_GDT<<2)+RPL3
#define SELECTOR_U_STACK SELECTOR_U_DATA

//定义3环段描述符
#define GDT_ATTR_HIGH ((DESC_G_4K<<7)+(DESC_D_32<<6)+(DESC_L<<5)+(DESC_AVL<<4))
#define GDT_ATTR_LOW_CODE_DPL3 ((DESC_P<<7)+(DESC_DPL_3<<5)+(DESC_S_CODE<<4)+DESC_TYPE_CODE)
#define GDT_ATTR_LOW_DATA_DPL3 ((DESC_P<<7)+(DESC_DPL_3<<5)+(DESC_S_DATA<<4)+DESC_TYPE_DATA)

/*TSS描述符属性*/
#define TSS_ATTR_LOW ((DESC_P<<7)+(DESC_DPL_0<<5)+(DESC_S_SYS<<4)+DESC_TYPE_AVALLABLE_TSS)
#define TSS_ATTR_HIGH ((DESC_G_4K<<7)+(DESC_AVL<<4))+(DESC_DB_32<<6)
typedef struct _gdt_desc
{
    uint16_t limit_low_word;
    uint16_t base_low_word;
    uint8_t base_mid_byte;
    uint8_t attr_low_byte;
    uint8_t limit_hight_attr_hight;
    uint8_t base_high_byte;
}gdt_desc;
/*定义EFLAGS*/
#define EFLAGS_MBS (1<<1)
#define EFLAGS_IF_1 (1<<9)//开中断
#define EFLAGS_IF_0 0   //关中断
#define EFLAGS_IOPL_3 (3<<12)//允许3环设置IO
#define EFLAGS_IOPL_0 0 //只允许0环设置IO


#endif

