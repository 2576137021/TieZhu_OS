#include "cat.h"
#include "../lib/use/syscall.h"
#include "../lib/stdio.h"
#include "../lib/kernel/string.h"

int main(int argc,char** argv){
    //管道限制,cat的输出不能大于等于环形缓冲区的大小，否则将会导致shell进程进入死锁
    if(argc == 1){
        char buf[512] = {0};
        read(0,buf,512);
        printf("%s",buf);
        exit(0);
    }
    if(argc !=2){
        printf("cat:error,Only one parameter is allowed\n");
        return -2;
    }
    int buf_size = 1024;
    char abs_path[512] = {0};
    void* buf = malloc(buf_size);
    if(buf ==NULL){
        printf("cat:error malloc failed\n");
    }
    if(argv[1][0] != '/'){
        getcwd(abs_path,buf_size);
        strcat(abs_path,"/");        
        strcat(abs_path,argv[1]);        
    }else{
        strcpy(abs_path,argv[1]);
    }
    int32_t fd = open(abs_path,O_READ);
    if(fd == -1){
        return -1;
    }
    int32_t read_bytes = 0;
    while(1){
        read_bytes = read(fd,buf,buf_size);
        if(read_bytes == -1){
            break;
        }
        write(1,buf,read_bytes);

    }
    free(buf);
    close(fd);
    return 1;
}