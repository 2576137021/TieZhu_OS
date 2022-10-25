#ifndef LIB_INTERRUPT_h
#define LIB_INTERRUPT_h
#include"stdint.h"
#define IDT_DESC_CNT 0x81 //已定义的中断数量
/******8259A主从片端口定义******/
#define PIC_M_CTRL 0x20 //主片控制端口
#define PIC_M_DATA 0x21 //主片数据端口
#define PIC_S_CTRL 0xa0 //从片控制端口
#define PIC_S_DATA 0xa1 //从片数据端口
typedef struct gate_desc/*中断门描述符结构体*/
{
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount;
    uint8_t attributes;
    uint16_t func_offset_high_word;
}Gate_desc,*PGate_desc;
enum  intr_status{
    INTR_OFF,
    INTR_ON
};
/*中断开关状态*/
typedef void* intr_handler;
static struct gate_desc idt[IDT_DESC_CNT];//中断描述符表
char* exceptionName[IDT_DESC_CNT];//异常名称数组
intr_handler idt_table[IDT_DESC_CNT];//中断处理程序数组
void idt_load(void);
extern intr_handler intr_entry_table[IDT_DESC_CNT];//kernel.s中断处理程序地址数组
enum intr_status intr_disable(void);//关闭中断
enum intr_status intr_enable(void);//启用中断
void intr_set_status(enum intr_status intrStatus);//设置中断状态
enum  intr_status get_intr_status(void);
enum intr_status intr_enable(void);
/*注册idt_table[vector]中断例程func*/
void register_handler(uint32_t vector,void* func);
#endif