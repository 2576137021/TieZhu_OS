#include "prog_arg.h"
#include "../lib/stdio.h"
#include "../lib/use/syscall.h"
#include "../lib/kernel/string.h"

int32_t main(int32_t argc,char** argv){
    printf("prog_arg running\n");
    int32_t arg_idx = 0;
    while(arg_idx < argc&& argv!=NULL){
        printf("argv[%d]:%s\n",arg_idx,argv[arg_idx]);
        arg_idx++;
    }
    int32_t pid = fork();
    if(pid){
        printf("father process,child pid:%d\n",pid);
        
    }else if(argv[1]!=0){
        //再执行一个磁盘程序
        char abs_path[512] ={0};
        printf("shou dao chan shu:%s\n",argv[1]);
        if (argv[1][0] !='/' )
        {   
            //因为execv没有解析路径的功能
            getcwd(abs_path,512);
            strcat(abs_path,"/");
            strcat(abs_path,argv[1]);
            printf("next arg:%s\n",abs_path);
            execv(abs_path,abs_path);
        }else{
             execv(argv[1],NULL);
        }
        
    }
    while(1);
}