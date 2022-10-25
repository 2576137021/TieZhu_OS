#include "shell.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/debug.h"
#include "../lib/stdio.h"
#include "../lib/use/syscall.h"
#include "../fs/file.h"
#include "buildin_cmd.h"
#include "../userprog/exec.h"
#include "../userprog/wait_exit.h"
#define cmd_len 128     //最长命令行输入
#define MAX_ARG_NR 16   //最多支持15个参数

static int32_t read_count; //shell中实际输出的字符数
/*储存输入的命令*/
static char cmd_line[cmd_len] = {0};
/*记录当前所在目录*/
char cwd_cache[MAX_PATH_LEN] = {0};
char* argv[MAX_ARG_NR];
char final_path[MAX_PATH_LEN];
int32_t argc= -1;


/*输出提示符*/
void print_prompt(void){
    int32_t cursor_pos;
    cursor(&cursor_pos);
    if((cursor_pos%80)!=0)printf("\n");
    printf("[TestUser@localhos %s]$ ",cwd_cache);
}
/*从键盘缓冲区读取count个字节到buf*/
static void readline(char* buf,int32_t count){
    read_count =0;
    assert(buf != NULL && count > 0);
    char* pos = buf;
    while(read(stdin_no,pos,1)!= -1 && (pos-buf)<count){
        switch(*pos){
            case '\n':
            case '\r':
                *pos =0;
                putchar('\n');
                return;
            
            case '\b':
                if(read_count!=0){
                    
                    pos--; //下次read的时候覆盖掉这次读取的符号
                    putchar('\b');
                    read_count--;
                }
                break;
            //参考keyboad的代码
            case 'l'-'a':
                *pos = 0;
                clear();
                print_prompt();
                break;
            case 'u'-'a':
                while(read_count){
                    putchar('\b');
                    pos--;
                    read_count--;
                }
                break;
            default:
                putchar(*pos);
                pos++;
                read_count++;
        }
         
    }
    printf("readline : cant't find entry key in the cmd_line,max num of char is 128\n");

}
/**
 * @brief 
 * 解析键入的字符串命令和参数,以token为分隔符，放置于argv中
 * 返回命令和参数总和,失败返回-1
 */
static int32_t cmd_parse(char* cmd_str,char** argv,char token){
    assert(cmd_str!= NULL);
    int32_t arg_idx =0;
    while(arg_idx < MAX_ARG_NR){
        argv[arg_idx] = NULL;
        arg_idx++;
    }

    char* next = cmd_str;
    int32_t argc =0;
    //外层循环处理整个命令行
    while(*next){
        //去除命令字和参数之间的空格
        while(*next == token){
            next++;
        }
        if(*next ==0){
            break;
        }
        argv[argc] = next;

        //内层循环处理命令行中的每个参数
        while(*next && *next != token){//在字符串结束前找单词分隔符
            next++;
        }

        if(*next){ //如果字符串没有结束
            *next++ =0; //将字符串结束,并指向被截断的字符串开始位置
        }

        //参数过多返回错误
        if(argc > MAX_ARG_NR){
            return -1;
        }
        argc++;

    }
    return argc;

}
/**
 * @brief 
 *比较内建和外部命令,并执行
 */
static void cmd_execute(uint32_t argc,char** argv){
        char* command = argv[0];
        //比较命令
        if(!strcmp(command,"ls")){
            buildin_ls(argc,argv);
        }else if(!strcmp(command,"pwd")){
            buildin_pwd(argc,argv);
        }else if(!strcmp(command,"cd")){
            char* new_path =buildin_cd(argc,argv);
            if(new_path){
                memcpy(cwd_cache,new_path,strlen(new_path));
            }
        }else if(!strcmp(command,"ps")){  //fork进程的时间没有被改变
                                          //因为运行时间太短,所以没有收到中断就被生产者模式暂停线程了
            buildin_ps(argc,argv);
        }else if(!strcmp(command,"clear")){
            buildin_clear(argc,argv);
        }else if(!strcmp(command,"mkdir")){
            buildin_mkdir(argc,argv);
        }else if(!strcmp(command,"rmdir")){ 
            buildin_rmdir(argc,argv);
        }else if(!strcmp(command,"rm")){
            buildin_rm(argc,argv);
        }else if(!strcmp(command,"exit")){
            exit(0);
        }else if(!strcmp(command,"help")){
            help();
        }else{
            /*第一个参数就是相对文件路径*/
            make_clear_abs_path(argv[0],final_path);
            argv[0] = final_path;
            struct _stat file_stat;
            memset(&file_stat,0,sizeof(file_stat));

            /*检查文件是否存在*/
            if(stat(argv[0],&file_stat)){
                int32_t pid = fork();
                if(pid){
                    //主进程,等待被shell加载的程序退出。
                    wait(NULL);
                }else{
                    //子进程
                    execv((const char*)argv[0],(const char**)argv);
                }
            }else{
                printf("shell:cannot access %s:No such file\n",argv[0]);
            }
            
        }
}




/*shell*/
void shell(void){
    cwd_cache[0] ='/';
    while(1){
        print_prompt();
        memset(final_path,0,sizeof(final_path));
        memset(cmd_line,0,sizeof(cmd_line));
        readline(cmd_line,sizeof(cmd_line));
        if(cmd_line[0] ==0)continue; //回车
        //管道处理
        char* pipe_symbol = strchar(cmd_line,'|');
        if(pipe_symbol){
            printf("pipe command\n");
            // 1.生成管道
            int32_t fd[2] = {-1}; //fd[0]输入,fd[1]输出
            if(!pipe(fd)){
                printf("create pipe error\n");
                break;
            }


            // 2.执行从左往右第一个命令
            char* each_cmd = cmd_line;
            pipe_symbol = strchar(cmd_line,'|');
            *pipe_symbol =0;
            argc = -1;
            argc = cmd_parse(each_cmd,argv,' ');
            // 3.在执行第一个命令之前,重定向标准输出到管道
            fd_redirect(stdout_no,fd[1]);
            // 4.cmd_execute不应该存在调用write的函数，避免将线程死锁
            cmd_execute(argc,argv);
            
            each_cmd = pipe_symbol +1;
            //  4.中间的命令,命令的输入来自管道，命令的输出写入管道
            fd_redirect(stdin_no,fd[0]);
            while((pipe_symbol = strchar(cmd_line,'|'))){
                *pipe_symbol =0;
                argc = -1;
                argc = cmd_parse(each_cmd,argv,' ');

                cmd_execute(argc,argv);
                
                each_cmd = pipe_symbol +1;
            }

            //  5.执行最后一个命令,还原标准输入输出
            //还原输出，让最后一条命令的输出能输出在屏幕上
            fd_redirect(stdout_no,stdout_no);
            argc = -1;
            argc = cmd_parse(each_cmd,argv,' ');
            cmd_execute(argc,argv);

            //还原输入,避免shell从管道中读取内容
            fd_redirect(stdin_no,stdin_no);
            //关闭管道
            close(fd[0]);
            close(fd[1]);
            continue;
        }
        argc=-1;
        argc = cmd_parse(cmd_line,argv,' ');
        if(argc ==-1){
            printf("Too many parameters\n");
            continue;
        }
        cmd_execute(argc,argv); //比较内建和外部命令,并执行
    }
    

}