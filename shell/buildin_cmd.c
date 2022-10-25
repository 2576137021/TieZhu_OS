#include "buildin_cmd.h"
#include "../fs/fs.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/string.h"
#include "../lib/stdio.h"
#include "../lib/use/syscall.h"
#include "../fs/file.h"
#include "shell.h"
/**
 * @brief 
 * old_abs_path 旧绝对路径
 * 转换旧绝对路径中的 ..和. 存入新绝对路径中
 */
static void wash_path(char* old_abs_path,char* new_abs_path){
    assert(old_abs_path[0] == '/');
    char name[MAX_FILE_NAME_LEN] = {0};
    char* sub_path = old_abs_path; //未解析的路径
    sub_path = path_parse(sub_path,name);
    if(name[0] == 0){
        // 只键入了 '/'的情况
        new_abs_path[0] = '/';
        new_abs_path[1] = 0;
        return;
    }
    new_abs_path[0] = 0;
    strcat(new_abs_path,"/");
    while(name[0]){
        if(!strcmp("..",name)){

            char* slash_ptr = strrchar(new_abs_path,'/');
            if(slash_ptr != new_abs_path){
                *slash_ptr = 0;
            }else{
                *(slash_ptr+1) = 0;
            }
        }else if(strcmp(".",name)){
            if(strcmp(new_abs_path,"/")){
                strcat(new_abs_path,"/");
            }
            strcat(new_abs_path,name);
        }
        memset(name,0,MAX_FILE_NAME_LEN);   
        if(sub_path){
            sub_path = path_parse(sub_path,name);
        }
    }
}
/**
 * @brief 
 * 解析路径(path)为绝对路径,final_path保存解析好的路径
 */
void make_clear_abs_path(char* path,char* final_pathv){
    char abs_path[MAX_PATH_LEN] = {0};
    memset(final_pathv,0,MAX_PATH_LEN);
    if(path[0] != '/'){
        //处理相对路径
        if(getcwd(abs_path,MAX_PATH_LEN) != NULL){
            if(strcmp(abs_path,"/")){
                //如果当前不是根目录
                strcat(abs_path,"/");
            }
        }
    }
    strcat(abs_path,path);
    wash_path(abs_path,final_pathv);
}
/**
 * @brief 
 * pwd命令的内部函数实现
 */
void buildin_pwd(int32_t argc,char** argv){
    if(argc !=1){
        printf("pwd:no argument support !\n");
    }else{
        if(getcwd(final_path,MAX_PATH_LEN)!= NULL){
            printf("%s\n",final_path);
        }else{
            printf("pwd:get current work directory failed\n");
        }
    }
}
/**
 * @brief 
 * cd 命令
 * 成功返回新路径,失败返回NULL
 */
char* buildin_cd(int32_t argc ,char** argv){
    if(argc >2){
        printf("cd:Only one parameter is supported\n");
        return NULL;
    }
    //只有 cd 一个参数,则进入根目录
    if(argc ==1){
        final_path[0] = '/';
        final_path[1] = 0;
    }else{
        make_clear_abs_path(argv[1],final_path);
    }
    if(chdir(final_path) == false){
        printf("cd: no such directory\n");
        return NULL;
    }
    return final_path;

}
/**
 * @brief 
 * ls 命令
 */
void buildin_ls(int32_t argc ,char** argv){


    char* pathname  = NULL;
    struct _stat file_stat;
    memset(&file_stat,0,sizeof(file_stat));

    bool long_info = false; //是否存在参数
    int32_t arg_path_nr = 0;
    int32_t arg_idx = 1; //跳过第一个命令 'ls'

    // 1.检查参数
    while(arg_idx <argc){
        char* cur_arg = argv[arg_idx];
        if(cur_arg[0] == '-')
        { //带参数解析
            long_info =true;
            if(!strcmp("-h",cur_arg))
            {
                // -h 参数
            printf("usage:\n\
-l list all infotmation about the file.\n\
-h for help.\n\
list all files in the current dirctory if no option\n");
                return;
            }else if(!strcmp("-l",cur_arg))
            {

            }
            else
            {
                printf("ls: invalid option %s\n",cur_arg);
                return;
            }
        }
        else
        {
            if(arg_path_nr == 0){
                pathname = cur_arg;
                arg_path_nr = 1;
            }else{
                printf("ls:only support one path\n");
                return;
            }
        }
        arg_idx++;
    }

    //2.解析路径
    if(pathname ==NULL){
        //只有ls 或 ls-l,使用当前路径作为绝对路径
        memset(final_path,0,sizeof(MAX_PATH_LEN));
        if(getcwd(final_path,MAX_PATH_LEN) != NULL){
            pathname = final_path;
        }else{
            printf("ls:getcwd error\n");
            return;
        }
    }else{
        make_clear_abs_path(pathname,final_path);
        pathname = final_path;
    }
    
    //3.遍历目录(if true),输出文件信息
    if(stat(pathname,&file_stat) == false)return;

    if(file_stat.st_file_type == FT_DIRECTORY)
    {
        //如果参数是目录则遍历目标目录项
        dir* dir = opendir(pathname);
        dir_entry* dir_e = NULL;
        char sub_pathnam[MAX_PATH_LEN] = {0};
        int32_t pathname_len = strlen(pathname);
        int32_t last_char_idx = pathname_len -1;
        memcpy(sub_pathnam,pathname,pathname_len);

        if(sub_pathnam[last_char_idx] !='/'){
            sub_pathnam[pathname_len] ='/';
            pathname_len++;
        }
        rewinddir(dir);

        if(long_info)
        {
            //存在参数
            char ftype;
            printf("total: %d\n",file_stat.st_size); //输出整个目录的大小
            while(dir_e = readdir(dir))
            {
                ftype = 'd';
                if(dir_e->f_type == FT_REGULAR)
                {
                    ftype = '-';
                }
                //
                sub_pathnam[pathname_len] = 0;
                strcat(sub_pathnam,dir_e->filename);
                //对于拼接过的路径再次解析
                make_clear_abs_path(sub_pathnam,final_path);
                memset(&file_stat,0,sizeof(struct _stat));
                if(!stat(final_path,&file_stat)){
                    //
                    printf("ls:%s,No such file or directory\n",final_path);
                    return;
                }
                printf("%c %d %d %s\n",ftype,dir_e->i_no,file_stat.st_size,dir_e->filename);
            }
        }else
        {
            //不存在参数
            while(dir_e = readdir(dir)){
                printf("%s  ",dir_e->filename);
            }
            printf("\n");
        }
        closedir(dir);
    }else
    {
        //如果是普通文件
        if(long_info){
            printf("- %d %d %s\n",file_stat.st_ino,file_stat.st_size,pathname);
        }else{
            printf("%s\n",pathname);
        }
    }

}

/**
 * @brief 
 * ps 命令
 */
void buildin_ps(int32_t argc,char** argv){
    if(argc != 1){
        printf("ps:Too many parameters\n");
        return;
    }
    ps();
}
/**
 * @brief 
 * clear 命令
 */
void buildin_clear(int32_t argc,char** argv){
    if(argc != 1){
        printf("clear:Too many parameters\n");
        return;
    }
    clear();
}
/**
 * @brief 
 * mkdir 命令
 * 成功返回true
 * 失败返回false
 */
int32_t buildin_mkdir(int32_t argc,char**argv){
    int32_t ret = false;
    if(argc != 2){
        printf("mkdir: only support 2 argument\n");
        return ret;
    }
    make_clear_abs_path(argv[1],final_path);
    if(!strcmp(final_path,"/")){
        printf("Please don't do this\n");
        return ret;
    }
    if(mkdir(final_path)){
        printf("%s\n",final_path);
        ret = true;
    }else{
        printf("mkdir: create directory %s failed\n",final_path); 
    }
    return ret;
}

/**
 * @brief 
 * rmdir 命令
 */
int32_t buildin_rmdir(int32_t argc, char** argv){
    int32_t ret = -1;
    if(argc != 2){
        printf("rmdir: only support 2 argument\n");
        return ret;
    }
    make_clear_abs_path(argv[1],final_path);
    if(!strcmp(final_path,"/")){
        printf("Please don't do this\n");
        return ret;
    }
    if(rmdir(final_path)){
        printf("%s\n",final_path);
        ret = true;
    }else{
        printf("rmdir: remove directory %s failed\n",final_path); 
    }
    return ret;
}
/**
 * @brief 
 * rm 命令
 */
int32_t buildin_rm(int32_t argc, char** argv){
    int32_t ret = -1;
    if(argc != 2){
        printf("rm: only support 2 argument\n");
        return ret;
    }
    make_clear_abs_path(argv[1],final_path);
    if(!strcmp(final_path,"/")){
        printf("Please don't do this\n");
        return ret;
    }
    if(unlink(final_path)){
        printf("%s\n",final_path);
        ret = true;
    }else{
        printf("rm: delete file %s failed\n",final_path); 
    }
    return ret;
}