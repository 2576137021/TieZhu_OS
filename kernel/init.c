#include "../lib/kernel/init.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/interrupt.h"
#include "../device/timer.h"
#include "../lib/kernel/memory.h"
#include "../thread/thread.h"
#include "../device/kerboard.h"
#include "../userprog/tss.h"
#include "../device/console.h"
#include "../userprog/syscall-init.h"
#include "../device/ide.h"
#include "../fs/fs.h"
void init_all(){
    print("init_all\n");
    idt_load();//保证初始化在定时器之前
    console_init();//初始化控制台设备
    mem_init();//初始化内存池
    init_timer();//初始化定时器,替换定时器中断处理程序
    init_main_thread();//初始化内核主线程pcb信息
    keyboard_init();//初始化键盘设备
    tss_init();//初始化TSS
    syscall_init();//初始化系统调用
    ide_init(); //初始化硬盘
    filesys_init();//初始化文件系统
}