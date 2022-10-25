#include "../lib/kernel/main.h"
// debug virtual address 0008:0xc0001500
// user code cs = 0x2b
void init(void);

uint32_t test_pid1 = 0;
uint32_t test_pid2 = 0;
 bool satatus;
void main(void ){
    asm volatile("cli");
    print("\nkernel thread start\n");
    //初始化
    init_all();
    intr_enable();
    cls_screen();
   

    print("kernel main thread pid:");
    put_int32(getpid());
    print("\n");

    // 将测试程序，写入到文件系统管理的磁盘中
    uint32_t file_size = 18016; //要写入磁盘的文件大小
    uint32_t sec_cnt = DIV_ROUND_UP(file_size,512);
    disk* sda = &channels[0].devices[0];
    void* buf = malloc(file_size);
    ide_read(sda,300,buf,sec_cnt);
    int32_t fd = sys_open("/cat",O_CREAT|O_WRITE);
    if(fd ==-1){
        dbg_printf("error fd\n");
    }else{
        if(sys_write(fd,buf,file_size) ==-1){
            dbg_printf("write error\n");
        }
    }
    //写入一个文本文件
    char key_buffer[30]="key_1998100419990502";
    key_buffer[strlen(key_buffer)] = 0;
    int32_t fd_2 = sys_open("/key.txt",O_CREAT|O_WRITE);
    if(fd_2 ==-1){
        dbg_printf("error fd2\n");
    }else{
        if(sys_write(fd_2,key_buffer,strlen(key_buffer)+1) ==-1){
            dbg_printf("write_error2\n");
        }
    }
    sys_close(fd);
    sys_close(fd_2);
   // cls_screen();
    print_prompt();
    thread_exit(get_running_thread_pcb(),true);

}

//第一个进程
void init(void){
    int32_t ret_pid  =  fork();
    if(ret_pid){
        printf("i am father,my pid is %d,child pid is %d\n",getpid(),ret_pid);
        while(1){
            //持续回收僵尸进程
            pid_t pid = wait(NULL);
            printf("recycle pid %d succeed\n",pid);
        }
    }else
    {   

        shell();
    }

}