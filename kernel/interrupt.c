//中断相关
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"
#define EFLAGS_IF 0x200 //IF位=1
#define GET_EFLAGS(EFLAGS_VALUE) asm volatile("pushfl;popl %0":"=q" (EFLAGS_VALUE))
#define SYS_CALL_IDT_NUMBER 0x80
extern uint32_t syscall_handler(void); //系统调用分发函数 定义在kernel.s中
static void general_intr_handler(uint8_t intr_vector){//通用中断处理程序
    if(intr_vector==0x27||intr_vector==0x2f){//忽略伪中断
        return;
    }
    set_cursor(0);
    uint16_t cursor_pos = 0;
    while (cursor_pos<320)
    {
       put_char(' ');
       cursor_pos++;
    }
    set_cursor(0);
    print("**************exception intr infomation**************\n");
    print("exceptionName:");
    print(exceptionName[intr_vector]);
    print("\n");
    if(intr_vector==14){
        //缺页异常,缺页地址
        uint32_t page_fault_vaddr = 0;
        asm volatile("mov %%cr2,%0":"=g" (page_fault_vaddr));
        print("page fault vaddr:");
        put_int32(page_fault_vaddr);
        print("\n");
    }
    print("**************exception intr infomation**************\n");
    print("\n");
    while (1)
    {
        //停止
    }
}
static void register_intr(void){//中断注册函数 
    int i;                  //初始化异常名称数组和中断处理程序数组
    for ( i = 0; i < IDT_DESC_CNT; i++)
    {   
         //待思考
         idt_table[i] =  general_intr_handler;//初始化我们定义的异常
         exceptionName[i] = "unknown";        //与上面无关,单纯的初始化异常名称数组。           
    }
   exceptionName[0] = "#DE Divide Error";
   exceptionName[1] = "#DB Debug Exception";
   exceptionName[2] = "NMI Interrupt";
   exceptionName[3] = "#BP Breakpoint Exception";
   exceptionName[4] = "#OF Overflow Exception";
   exceptionName[5] = "#BR BOUND Range Exceeded Exception";
   exceptionName[6] = "#UD Invalid Opcode Exception";
   exceptionName[7] = "#NM Device Not Available Exception";
   exceptionName[8] = "#DF Double Fault Exception";
   exceptionName[9] = "Coprocessor Segment Overrun";
   exceptionName[10] = "#TS Invalid TSS Exception";
   exceptionName[11] = "#NP Segment Not Present";
   exceptionName[12] = "#SS Stack Fault Exception";
   exceptionName[13] = "#GP General Protection Exception";
   exceptionName[14] = "#PF Page-Fault Exception";
   // exceptionName[15] 第15项是intel保留项,未使用
   exceptionName[16] = "#MF x87 FPU Floating-Point Error";
   exceptionName[17] = "#AC Alignment Check Exception";
   exceptionName[18] = "#MC Machine-Check Exception";
   exceptionName[19] = "#XF SIMD Floating-Point Exception";
}
/**********8259A初始化**********/
static void pic_init(void){
    /*
        先初始化主从片ICM1-4
    */
    //初始化主片 ICW
    outb(PIC_M_CTRL,0x11);
    outb(PIC_M_DATA,0x20); //设置起始中断向量号0x20开始顺延至0x27
    outb(PIC_M_DATA,0x4);//IRQ2接从片
    outb(PIC_M_DATA,0x1); //????不是应该设置主片吗？
    //初始化从片 ICW
    outb(PIC_S_CTRL,0x11);
    outb(PIC_S_DATA,0x28);//设置起始中断向量号0x28开始顺延至0x2F
    outb(PIC_S_DATA,0x2);//指示主片IRQ2连接此从片
    outb(PIC_S_DATA,0x1);
    
    /*开启时钟中断、键盘中断、从片级联 OCW*/
    outb(PIC_M_DATA,0xf8);//开启IRQ0中断,即时钟中断,为什么是时钟中断?,因为计数器通过out0数据线接到8259A的IRQ0上这是设计好的,8259A的IRQ0必定是时钟中断
    outb(PIC_S_DATA,0xBF);//开启硬盘中断
    
    print("pic_init success\n");

}
/**********IDT初始化**********/
//创建一个中断门描述符
static void make_idt_desc(PGate_desc p_gdesc,uint8_t attr,intr_handler function){
    p_gdesc->func_offset_low_word =(uint32_t)function;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attributes = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function&0xffff0000)>>16;
}
//初始化已定义的中断描述符表
static void idt_init(void){
    int i;
    for ( i = 0; i < 0x30; i++)//intr_entry_table中只保存了0x30个中断例程的入口地址
    {
        make_idt_desc(&idt[i],IDT_ATTRIBUTE,intr_entry_table[i]);
    }
    make_idt_desc(&idt[SYS_CALL_IDT_NUMBER],(IDT_P<<7)|(IDT_DPL3<<5)|(IDT_S<<4)|(IDT_TYPE),syscall_handler);//
}
/*初始化中断向量表*/
void idt_load(void){
    
    idt_init();
    register_intr();//初始化中断处理函数数组
    pic_init();//初始化8259A
    uint64_t idt_IDTR = (sizeof(idt)-1) | ((uint64_t)(uint32_t)idt<<16);
    asm volatile("lidt %0": :"m"(idt_IDTR));//加载idt
    print("idt_load success\n");
}
/*断言方法*/
/*获取中断状态*/
enum  intr_status get_intr_status(void){
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (eflags&EFLAGS_IF?INTR_ON:INTR_OFF);
}
/*开启中断,返回之前的中断状态*/
enum intr_status intr_enable(void){
    enum intr_status old_status = get_intr_status();
    if (old_status==INTR_OFF)
    {
        asm volatile ("sti");
    }
    return old_status;
    
}
/*关闭中断,返回之前的中断状态*/
enum intr_status intr_disable(void){
    enum intr_status old_status = get_intr_status();
    if (old_status==INTR_ON)
    {
        asm volatile ("cli");
    }
    return old_status;  
}
/*设置中断状态*/
void intr_set_status(enum intr_status intrStatus){
    if(intrStatus==INTR_ON){
        asm volatile ("sti");
    }else{
        asm volatile ("cli");
        
    }
}
/*注册idt_table[vector]中断例程func*/
void register_handler(uint32_t vector,void* func){
    idt_table[vector]=func;
}
