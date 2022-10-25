/**
 * @file prog_pipe.c
 * @author your name (you@domain.com)
 * @brief 使用管道通信的父子进程例子
 * @version 0.1
 * @date 2022-10-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "../lib/stdio.h"
#include "../lib/kernel/stdint.h"
#include "../userprog/fork.h"
#include "../shell/pipe.h"
#include "../lib/use/syscall.h"
int32_t main(int32_t argc,char* argv[]){
    int32_t fd[2] ={-1};
    if(!pipe(fd)){
        printf("pipe error\n");
        return 0;
    }
    pid_t child_pid = fork();
    if(child_pid){
        //父进程
        close(fd[0]);
        write(fd[1],"A message from the parent process\n",34);
        printf("father,my pid is %d\n",getpid());
        if(wait(NULL)==-1){
            printf("wait error\n");
        }
        printf("father exit\n");
        return 0;
    }else{
        //子进程
        close(fd[1]);
        char buffer[50] ={0};
        read(fd[0],buffer,34);
        printf("child,my pid %d\n",getpid());
        printf("child,fathed send message:%s\n",buffer);
        return 0;
    }
}

