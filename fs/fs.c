#include "fs.h"
#include "inode.h"
#include "super_block.h"
#include "dir.h"
#include "file.h"
#include "../device/ide.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../device/console.h"
#include "../device/ioqueue.h"
#include "../device/kerboard.h"
#include "../shell/pipe.h"
#include "../lib/stdio.h"
partition* cur_part;//默认情况下操作的是哪个分区
int32_t fd_idx2global_ftable_idx(uint32_t fd_idx);
/*记录查找文件过程中已找到的上级路径*/
typedef struct _path_seach_record{
    char searched_path[MAX_PATH_LEN]; //保存已搜索的路径，如果'断链'则保存到断链路径为止
    dir* parent_dir;                   //父目录
    enum file_type file_type;         //文件类型，不存在则为 FT_UNKNOW
}path_seach_record;

/**
 * @brief 
 * 在分区链表中找到名为part_name的分区，并将其指针赋值给cur_part
 */
static bool mount_partition(list_elem* pelem,void* arg){
    dbg_printf("mount_partition run\n");
    char* part_name = (char*)arg;
    partition* part = (partition*)struct_entry(pelem,partition,part_tag);
    dbg_printf("part_name:%s,part->name:%s\n",part_name,part->name);
    if(strcmp(part->name,part_name)==0){
        cur_part = part;
        disk* hd = cur_part->my_disk;

        /*读取分区超级块*/
        super_block* sb_buf = sys_malloc(sizeof(super_block));
        if(cur_part->sb ==NULL){
            cur_part->sb = (super_block* )sys_malloc(sizeof(super_block));
        }

        if(sb_buf==NULL||cur_part->sb ==NULL){
            dbg_printf("sys_malloc error\n");
            return;
        }
        //将硬盘中的超级块赋值给内存中的超级块
        memset(sb_buf,0,SECTOR_SIZE);//super_block size = super_block
        ide_read(hd,cur_part->start_lba+1,sb_buf,1);
        memcpy(cur_part->sb,sb_buf,SECTOR_SIZE);

         //复制硬盘中的block位图到buf
        cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects*SECTOR_SIZE);
        cur_part->block_bitmap.bitmap_byte_len = sb_buf->block_bitmap_sects*SECTOR_SIZE;
        ide_read(hd,sb_buf->block_bitmap_lba,cur_part->block_bitmap.bits,sb_buf->block_bitmap_sects);

        dbg_printf("block bitmap value:%x\n",*cur_part->block_bitmap.bits);
        

        /*读取硬盘中的inode位图到buf*/
        cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects*SECTOR_SIZE);
        cur_part->inode_bitmap.bitmap_byte_len = sb_buf->inode_bitmap_sects *SECTOR_SIZE;
        ide_read(hd,sb_buf->inode_bitmap_lba,cur_part->inode_bitmap.bits,sb_buf->inode_bitmap_sects);
        list_init(&cur_part->open_inodes);
        sys_free(sb_buf);
        
        dbg_printf("mount %s done!\n",part->name);
        return true;
    }
    return false;
}
/**
 * @brief 
 * 格式化分区,创建文件系统
 */
static void partition_format(partition* part){
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;

    //inode位图占用的扇区数
    uint32_t inode_bitmap_sector = DIV_ROUND_UP(MAX_FILES_PER_PART,BITS_PER_SECTOR);

    //inode_table 占用的扇区数
    uint32_t inode_table_sects = DIV_ROUND_UP((sizeof(inode)*MAX_FILES_PER_PART),SECTOR_SIZE);

    //已使用的扇区数
    uint32_t used_sects = boot_sector_sects+super_block_sects+inode_table_sects+inode_bitmap_sector;

    //空闲扇区数
    uint32_t free_sects = part->sec_cnt- used_sects;

    //空闲块位图占据的扇区数
    uint32_t block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(free_sects,BITS_PER_SECTOR);//当前空闲块位图需要多少个扇区

    uint32_t block_bitmap_bit_len = free_sects-block_bitmap_sects;//空闲块位图的长度
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len,BITS_PER_SECTOR);//一位= 一块,向上取整，多余的部分会多占用一个扇区.

    /*超级块初始化*/
    super_block sb;
    sb.magic = 0x10190498;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;
    
    sb.block_bitmap_lba = sb.part_lba_base+2; //0扇区是引导块,1扇区是超级块,2扇区是block位图
    sb.block_bitmap_sects = block_bitmap_sects;

    sb.inode_bitmap_lba = sb.block_bitmap_lba+sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sector;

    sb.inode_table_lba = sb.inode_bitmap_lba+sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba+sb.inode_table_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(dir_entry);

// dbg_printf("%s info:\n",part->name);
//     dbg_printf("    magic:0x%x\n    part_lba_base:0x%x\n    all_sectors:0x%x\n    \
// inode_cnt:0x%x\n    block_bitmap_lba:0x%x\n    block_bitmap_sectors:0x%x\n    \
// inode_bitmap_lba:0x%x\n    inode_bitmap_sectors:0x%x\n    \
// inode_table_lba:0x%x\n    inode_table_sectors:0x%x\n    \
// data_start_lba:0x%x\n", \
    sb.magic,sb.part_lba_base,sb.sec_cnt,sb.inode_cnt,sb.block_bitmap_lba,sb.block_bitmap_sects,\
    sb.inode_bitmap_lba,sb.inode_bitmap_sects,sb.inode_table_lba,\
    sb.inode_table_sects,sb.data_start_lba);

    //将填充好的超级块从栈中写入到磁盘
    disk* hd = part->my_disk;
    ide_write(hd,part->start_lba+1,&sb,1);//超级块写入第二个扇区

    //为位图和inode_table申请栈空间缓冲区
    //异常:未知的操作码。改变一下写法
    uint32_t buf_size = 0;
    if(block_bitmap_sects>=inode_bitmap_sector){
        buf_size = block_bitmap_sects;
    }else{
        buf_size = inode_bitmap_sector;
    }
    if(buf_size<inode_table_sects){
        buf_size = inode_table_sects;
    }
    // buf_size =block_bitmap_sects>=inode_bitmap_sector?block_bitmap_sects:inode_bitmap_sector;
    // buf_size = buf_size>=inode_table_sects?buf_size:inode_table_sects;

    uint8_t* buf = (uint8_t*)sys_malloc(buf_size*SECTOR_SIZE);
    uint32_t buf_readl_size = buf_size*SECTOR_SIZE;
    if(buf==NULL){
        dbg_printf("error:fs.c_partition_format_sys_malloc\n");
        return;
    }
    /*初始化空闲块位图并写入磁盘*/

    buf[0] = 0x1; //预留第一个空闲块,给根目录占用
    //填充位图向上取整时占用的一个扇区的空闲部分.
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8; //占用了多少个字节
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;   //不足一字节的部分
    uint32_t last_size = SECTOR_SIZE -(block_bitmap_last_byte%SECTOR_SIZE);//空闲块位图最后一个扇区中,空闲块位图未使用的大小


    memset(&buf[block_bitmap_last_byte+1],0xff,last_size); //扇区的剩余部分全部置1表示不可用

    uint8_t bit_idx = 0;
    while (bit_idx<= block_bitmap_last_bit)              //恢复不足一字节的部分
    {                                                   //书上这里用了<=,但我认为应该是<
        buf[block_bitmap_last_byte+1]&=~(1<<bit_idx++); //某一位置零
    }

    dbg_printf("block bitmap lba:%x\n",sb.block_bitmap_lba);
    ide_write(hd,sb.block_bitmap_lba,buf,sb.block_bitmap_sects);
    
    /*初始化inode位图并写入磁盘*/
    memset(buf,0,buf_size);
    buf[0] = 0x1; //第一个inode 预留给inode占用
    /*
        因为我们设计的系统单个分区容纳4096个文件,即4096个inode
        inode位图一位代表1个inode,刚好4096字节 = 1扇区
    */
   dbg_printf("inode bitmap lba:%x\n",sb.inode_bitmap_lba);
   ide_write(hd,sb.inode_bitmap_lba,buf,sb.inode_bitmap_sects);

   /*初始化inode数组并写入磁盘*/
   memset(buf,0,buf_size);
   inode* i = (inode*)buf;
   //写入根目录的inode
   i->i_bsize = sb.dir_entry_size*2; // '.'和'..'目录
   i->i_no = 0;
   i->i_sectors[0] =sb.data_start_lba;//根目录的第一个物理块就在空闲起始地址
   
   dbg_printf("inode table lba:%x\n",sb.inode_table_lba);
   ide_write(hd,sb.inode_table_lba,buf,inode_table_sects);

   /*写入目录项 '.' */
   memset(buf,0,buf_size);

   dir_entry* p_de = (dir_entry*)buf;
   memcpy(p_de->filename,".",2);
   p_de->f_type = FT_DIRECTORY;
   p_de->i_no = 0;
   /*写入目录项 '..' */
   p_de++;//跳转到第二目录项
   
   /*写入目录项 '..'*/
   memcpy(p_de->filename,"..",2);
   p_de->f_type = FT_DIRECTORY;
   p_de->i_no = 0; //根目录的父目录 = 根目录自己

   ide_write(hd,sb.data_start_lba,buf,1); //目录项写入到空闲起始扇区

    //同步block位图
    memset(buf,0,buf_readl_size);
    *buf =3;
    ide_write(hd,sb.block_bitmap_lba,buf,1);
    dbg_printf("block bitmap write value:%d\n",buf);
   // dbg_printf("root_dir_lib:0x%x\n",sb.data_start_lba);
   // dbg_printf("%s format done\n",part->name);
    sys_free(buf);
}
/**
 * @brief 
 * 在通道上搜索磁盘，在磁盘上搜索文件系统,如果不存在则格式化分区创建文件系统
 */
static void search_filesys(void){
     uint8_t channel = 0,dev_no,part_idx = 0;
    super_block* sb_buf = (super_block*)sys_malloc(sizeof(super_block));
    if(sb_buf ==NULL){
        dbg_printf("fs.c_filesys_init_sysmalloc_error\n");
        return;
    }
    while (channel<channel_cnt)
    {
        dev_no =0;
        while (dev_no<2)
        {
            if(channel==0&&dev_no==0){//内核所在磁盘,不创建文件系统
                dev_no++;
                continue;
            }
            disk* hd = &channels[channel].devices[dev_no];
            partition* part = hd->prim_parts;
            while (part_idx<12) //4主分区+8逻辑分区
            {
                if(part_idx == 4){ //逻辑分区
                    part= hd->logic_parts;
                }
                if(part->sec_cnt!=0){//是否存在分区,必须保证初始化为0
                    memset(sb_buf,0,sizeof(super_block));
                    //读取超级块
                    ide_read(hd,part->start_lba+1,sb_buf,1);
                    //检查是否存在文件系统
                    if(sb_buf->magic==0x10190498){
                        dbg_printf("%s has filesystem\n",part->name);
                    }else{
                        dbg_printf("formatting %s's partition %s ......\n",hd->name,part->name);
                        partition_format(part);
                    }
                }
                part++;//跳转到下一个分区 bug 没有写++
                part_idx++;
            }
            dev_no++;
        }
        channel++;
    }
    sys_free(sb_buf);
}
/**
 * @brief 
 * 将最上层路径名解析出来
 * patname:完整路径
 * name_store：保存最上层路径
 * 路径为空返回NULL,其余返回patname
 */
char* path_parse(char* patname,char* name_store){
    if(patname[0]==0){//路径为空,返回NULL
        return NULL;
    }
    if(patname[0] =='/'){
        //跳过根目录和连续的 '/'路径解析
        while (*(++patname)=='/');         
    }
    while ((*patname != '/') && (*patname != 0))
    {
        *name_store++ = *patname++;
    }
    return patname;//此时patname指向下一级目录 /b/c或字符串结尾
}
/**
 * @brief 
 * 返回路径深度,/a/b/c的路径深度为3
 */
int32_t path_depth_cnt(const char* pathname){
    assert(pathname!=NULL);
    char* p = (char*)pathname;
    char name[MAX_FILE_NAME_LEN] = {0};

    uint32_t depth = 0;

    /*解析路径，从中拆分出各级名称*/
    p = path_parse(p,name);
    while(name[0]){
        depth++;
        memset(name,0,MAX_FILE_NAME_LEN);
        p = path_parse(p,name);
    }
    return depth;
}
/**
 * @brief 
 * 搜索文件，成功返回其inode编号并打开其父目录，失败返回-1
 * pathname: 文件完整路径(从根目录开始)
 */
static int32_t search_file(const char* pathname,path_seach_record* searched_record){
    //如果是根目录，直接返回已知根目录信息
    if((!strcmp(pathname,"/"))|| (!strcmp(pathname,"/.")) || (!strcmp(pathname,"/.."))){
        searched_record->file_type = FT_DIRECTORY;
        searched_record->parent_dir = &root_dir;
        memset(searched_record->searched_path,0,MAX_PATH_LEN);
    }

    //路径长度检查和格式'/x/x'检查
    uint32_t path_len = strlen(pathname);
    assert(path_len>=1&&path_len<=MAX_PATH_LEN&&pathname[0] == '/');

    char* sub_path = (char*)pathname;
    dir* parent_dir = &root_dir;
    dir_entry dir_e;

    //保存各级目录名称
    char name[MAX_FILE_NAME_LEN] = {0};

    searched_record->file_type = FT_UNKNOW;
    searched_record->parent_dir = parent_dir;
    uint32_t parent_inode_no = 0; //父目录的inode号

    sub_path = path_parse(sub_path,name);
    while (name[0])//如果路径不为空
    {
        /* 记录已存在的父目录 */
        strcat(searched_record->searched_path,"/");
        strcat(searched_record->searched_path,name);

        //
        if(search_dir_entry(cur_part,parent_dir,name,&dir_e))
        {
            memset(name,0,sizeof(name));
            if(sub_path){
                sub_path = path_parse(sub_path,name);
            }
            if(dir_e.f_type==FT_DIRECTORY){ //如果是目录
                parent_inode_no = parent_dir->inode->i_no;
                dir_close(parent_dir);
                parent_dir = dir_open(cur_part,dir_e.i_no);
                searched_record->parent_dir = parent_dir;
                continue;
            }else if(dir_e.f_type == FT_REGULAR){//是普通文件,如果路径未完就直接无视了
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
            
        }
        else
        {//找不到这样的目录或文件
            return -1;
        }
    }
    //走到此处，说明遍历了全路径，只可能是目录
    dir_close(searched_record->parent_dir);//此时parent_dir = 当前目录,关闭当前目录

    //更新文件搜索结构体
    searched_record->parent_dir = dir_open(cur_part,parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
    
}
/**
 * @brief 
 * 创建文件 
 * 成功返回文件描述符数组下标,失败返回-1
 */
static int32_t file_create(dir* parent_dir,char* filename,uint8_t flag){
    
    //公共缓冲区
    void* io_buf = sys_malloc(1024);//作为磁盘同步的缓冲区，可能存在跨扇区情况所以是两个扇区大小
    if(io_buf==NULL){
        dbg_printf("!!!file_create sys_malloc error\n");
        return -1;
    }

    //回滚标志位
    uint8_t rollback_setp = 0 ;

    //为文件分配inode编号
    int32_t inode_no = inode_bitmap_alloc(cur_part);
    if(inode_no == -1){
        dbg_printf("inode_bitmap_alloc error\n");
        sys_free(io_buf);
        return -1;
    }

    //创建inode结构体,此结构体必须位于堆中
    inode* new_file_inode = sys_malloc(sizeof(inode));
    if(new_file_inode==NULL){
        dbg_printf("!!!file_create:sys_malloc error\n");
        rollback_setp = 1;
        goto rollback;
    }
    inode_init(inode_no,new_file_inode);

    //加入全局文件表
    int32_t fd_idx = get_free_slot_in_file_table();
    if(fd_idx==-1){
        dbg_printf("!!!file_create:get_free_slot_in_file_table error\n");
        rollback_setp =2;
        goto rollback;
    }
    file_table[fd_idx].fd_flag= flag;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_inode->write_deny =false;

    //创建目录项
    dir_entry new_dir_entry;
    memset(&new_dir_entry,0,sizeof(dir_entry));
    create_dir_entry(filename,inode_no,FT_REGULAR,&new_dir_entry);

    //同步父目录
   if(!sync_dir_entry(parent_dir,&new_dir_entry,io_buf)){
        dbg_printf("!!!file_create: sync_dir_entry error\n");
        rollback_setp =3;
        goto rollback;       
    }
    memset(io_buf,0,1024);
    inode_sync(cur_part,parent_dir->inode,io_buf );
    
    memset(io_buf,0,1024);
    inode_sync(cur_part,new_file_inode,io_buf);

    memset(io_buf,0,1024);
    bitmap_sync(cur_part,inode_no,INODE_BITMAP);

    list_push(&cur_part->open_inodes,&new_file_inode->inode_tag);
    new_file_inode->i_open_cnts += 1;

    sys_free(io_buf);
    //安装到线程文件描述符表中
    return pcb_fd_install(fd_idx);


    //创建文件失败后的回滚操作
rollback:
    switch(rollback_setp){
        case 3:
        {   
            //还原全局文件表
            memset(&file_table[fd_idx],0,sizeof(file));
        }
        case 2:
        {   
            sys_free(new_file_inode);
        }
        case 1:
        {   
            //恢复inode位图位
            bitmap_set(&cur_part->inode_bitmap,inode_no,0);
            sys_free(io_buf);
        }

    }
    return -1;
}
/**
 * @brief 
 * 打开inode编号为innode_no的文件
 * 成功返回文件描述符，失败返回-1
 */
static int32_t file_open(uint32_t inode_no,uint8_t flag){
    int fd_idx = get_free_slot_in_file_table();
    if(fd_idx ==-1){
        dbg_printf("max openfile\n");
        return -1;
    }
    file_table[fd_idx].fd_inode = inode_open(cur_part,inode_no);
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;

    bool* write_deny_ptr = &file_table[fd_idx].fd_inode->write_deny;
    bool write_deny = *write_deny_ptr;

    if(flag&O_WRITE||flag&O_RW){
        enum intr_status old_status = intr_disable();
        if(!write_deny){
            //没有其他进程在写入文件
            *write_deny_ptr = true;
        }else{
            dbg_printf("This file is being used by another process or thread, please try again later\n");
            return -1;
        }
        intr_set_status(old_status);
    }

    return pcb_fd_install(fd_idx);
}

/**
 * @brief 
 * 关闭文件，全局文件表值恢复为默认值
 */
static bool file_close(file* file){
    if(file ==NULL){
        return false;
    }
    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode =NULL; //恢复全局文件表中可用性
    return 0;
}
/**
 * @brief 
 * 将buf中的count个字节写入到file
 * 默认情况不覆盖源文件,数据添加到文件末尾
 * 成功返回写入的字节数，失败返回-1
 */
static int32_t file_write(file* file,const void* buf, uint32_t count){
    if((file->fd_inode->i_bsize+count)>(BLOCK_SIZE *140)){
        dbg_printf("write error:file size to max");
        return -1;
    }

    uint8_t* io_buf = sys_malloc(512);
    if(io_buf == NULL){
        dbg_printf("sys_malloc io_buf error\n");
        return -1;
    }

    uint32_t* all_block = (uint32_t*)sys_malloc(BLOCK_SIZE+12*4);
    if(all_block ==NULL){
        dbg_printf("all_block malloc error\n");
        return -1;
    }
    void* new_io_buf =  sys_malloc(BLOCK_SIZE*2);
    if((int32_t)new_io_buf==-1){
        dbg_printf("fs_write:sys_malloc error\n");
        return -1;
    }

    const uint8_t* src = buf; //待写入的数据
    uint32_t bytes_written = 0; //已写入数据大小
    uint32_t size_left = count; //未写入数据大小
    int32_t block_lba = -1 ;//块地址
    int32_t block_bitmap_idx = 0; 
    uint32_t sec_idx; //用于索引扇区
    uint32_t sec_lba;//扇区地址
    uint32_t sec_off_bytes; //扇区内字节偏移
    uint32_t sec_left_bytes; //扇区内剩余字节量
    uint32_t chunk_size; //每次写入硬盘的数据块大小
    int32_t indirect_block_table; //一级间接表
    uint32_t block_idx; //块索引
    uint32_t* file_sectors = (uint32_t*)&file->fd_inode->i_sectors; //文件blocks
    /*判断文件是否有直接数据块*/
    if(file->fd_inode->i_sectors[0] ==0){
        dbg_printf("file_write:first write\n");
        //第一次写入文件
        block_lba = block_bitmap_alloc(cur_part);
        if(block_lba ==-1){
            dbg_printf("file_write block_bitmap_alloc error\n");
            sys_free(io_buf);
            return -1;
        }
        file->fd_inode->i_sectors[0] = block_lba;

        /*每分配一个块就将位图同步到硬盘*/
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        assert(block_lba!=0);
        bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
    }
    uint32_t file_has_size = file->fd_inode->i_bsize;
    //写入前，已占用的block
    uint32_t file_has_used_blocks =file_has_size/BLOCK_SIZE+1;
    
    //计算写入后，该文件占用的块数
    uint32_t file_will_use_blocks = (file_has_size+count)/BLOCK_SIZE +1;
    dbg_printf("file_write:reality write size:%d,file_will_use_blocks:%d\n", (file_has_size+count),file_will_use_blocks);
    assert(file_will_use_blocks<=140);

    //增加的块数
    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

    // 1.不分配新的扇区
    if(add_blocks ==0)
    {
        
        if (file_will_use_blocks <=12)
        {
            //使用直接块
            block_idx = file_has_used_blocks -1;
            all_block[block_idx] = file->fd_inode->i_sectors[block_idx];
        }else{
            //使用间接块
            assert(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
    
            ide_read(cur_part->my_disk,indirect_block_table,all_block+12,1);
        }
    }
    // 2.要分配新的扇区，共有三种情况分别处理
    else
    {
        

        // 2.1 第一种情况 新数据可直接存储在12个直接块中
        if(file_will_use_blocks <=12)
        {
            block_idx = file_has_used_blocks -1;

            all_block[block_idx] = file->fd_inode->i_sectors[block_idx];

            //分配扇区并写入到all_blocks
            block_idx = file_has_used_blocks;
            while (block_idx < file_will_use_blocks)
            {
                /* code */
                block_lba = block_bitmap_alloc(cur_part);
                if(block_lba==-1){
                    dbg_printf("file_write:block_bitmap_alloc error\n");
                    return -1;
                }
                //文件被删除时,块地址被清零
                assert(file_sectors[block_idx]==0);

                //写入到blocks数组中
                file_sectors[block_idx] = all_block[block_idx] = block_lba;

                //同步blocks位图到磁盘中
                block_bitmap_idx = block_lba-cur_part->sb->data_start_lba;
                bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
                block_idx++;
            }
            
        }
        // 2.2 第二种情况 旧数据使用12个直接块,新数据使用间接块+直接块或只使用间接块
        else if(file_has_used_blocks <=12 && (file_will_use_blocks>12))
        {
            /*先将*/
            block_idx = file_has_used_blocks-1;
            all_block[block_idx] = file_sectors[block_idx];

            //因为旧数据没有占用一级间接块,所以一级间接块肯定为空
            block_lba = block_bitmap_alloc(cur_part);
            if(block_lba ==-1){
                dbg_printf("file_write:block_bitmap_alloc error\n");
                return -1;
            }
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
            assert(file_sectors[12]==0);
            //分配一级间接表
            indirect_block_table = file_sectors[12] = block_lba;

            //block_idx,未使用的第一个块下标
            block_idx = file_has_used_blocks;
            while(block_idx<file_will_use_blocks){
                block_lba = block_bitmap_alloc(cur_part);
                if(block_lba ==-1){
                    dbg_printf("fs_write:block_bitmap_alloc error\n");
                    return -1;
                }
                if(block_idx< 12){
                    //新创建的块属于直接块,因为实际上file_sectors并没有分配间接块(只包含13个数组元素)，所以只能同步直接块
                    assert(file_sectors[block_idx] ==0);
                    file_sectors[block_idx] =all_block[block_idx]= block_lba;
                }else{
                    //属于间接块, 写入到allblocks 等待分配完成后一次性写入到硬盘
                    all_block[block_idx] = block_lba;
                }
                //将新分配的块位图信息同步到磁盘
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
                block_idx++;
            }
            //同步一级间接表到磁盘,all_block此时并没有同步
            ide_write(cur_part->my_disk,indirect_block_table,all_block+12,1);
        }
        // 2.3 第三种情况 原始数据已经占用了一级间接块
        else if(file_has_used_blocks>12){
            assert(file_sectors[12] != 0);

            indirect_block_table = file_sectors[12];
            
            //读取间接表到all_block
            ide_read(cur_part->my_disk,indirect_block_table,all_block+12,1);
            
            //第一个未被使用的block 下标
            block_idx = file_has_used_blocks;
            while (block_idx<file_will_use_blocks)
            {
                block_lba = block_bitmap_alloc(cur_part);
                if(block_lba==-1){
                    dbg_printf("fs_write:block_bitmap_alloc error\n");
                    return -1;
                }
                all_block[block_idx++] = block_lba;
                //同步block 位图到硬盘
                block_bitmap_idx = block_lba- cur_part->sb->data_start_lba;
                bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
            }
            //同步一级间接表到硬盘，all_block此时并没有同步
            ide_write(cur_part->my_disk,indirect_block_table,all_block+12,1);
        }
    }

    //此时，要使用到的地址都存放在all_block中
    bool first_write_block = true; //含有剩余空间的块标识

    file->fd_pos = file->fd_inode->i_bsize-1;//指向文件最后一个字节+1
    dbg_printf("write lba:0x%x\n",all_block[file->fd_inode->i_bsize / BLOCK_SIZE]);
    while(bytes_written < count){
        //各项写入数据
        memset(io_buf,0,BLOCK_SIZE);
        sec_idx = file->fd_inode->i_bsize / BLOCK_SIZE;
        sec_lba = all_block[sec_idx];
        dbg_printf(" 0x%x ",sec_lba);
        sec_off_bytes = file->fd_inode->i_bsize %BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE-sec_off_bytes;
        //判断写入硬盘的数据大小
        if(size_left<sec_left_bytes){ //不使用三目 
            chunk_size = size_left;
        }else{
            chunk_size =  sec_left_bytes;
        }

        if(first_write_block){
            ide_read(cur_part->my_disk,sec_lba,io_buf,1);
            first_write_block = false;
        }
        memcpy(io_buf + sec_off_bytes,(char*)src,chunk_size);
        ide_write(cur_part->my_disk,sec_lba,io_buf,1);


        src+=chunk_size; //移动写入指针
        file->fd_inode->i_bsize += chunk_size;//更新文件大小
        file->fd_pos +=chunk_size;
        bytes_written += chunk_size;
        size_left -= chunk_size;
    }
    inode_sync(cur_part,file->fd_inode,new_io_buf);
    sys_free(new_io_buf);
    sys_free(io_buf);
    sys_free(all_block);

    return bytes_written;
}
/**
 * @brief 
 * 将buf中连续的count字节写入到文件描述符为fd的文件
 * 默认情况不覆盖源文件,数据添加到文件末尾
 * 成功返回写入的字节数，失败返回-1
 */
int32_t sys_write(int32_t fd,const void* buf,uint32_t count){
    if(count==0){
        return 0;
    }
    if(fd<0){
        dbg_printf("sys_write:fd error\n");
        return -1;
    }
    //管道
    if(is_pipe(fd))return pipe_write(fd,buf,count);
    if(fd ==stdout_no){
        char temp_buf[1024] = {0};
        memcpy(temp_buf,(void*)buf,count);
        console_put_str(temp_buf);
        return count;
    }
    uint32_t _fd = fd_idx2global_ftable_idx(fd);
    file* wr_file = &file_table[_fd];
    if(wr_file->fd_flag& O_RW||wr_file->fd_flag&O_WRITE){
        return file_write(wr_file,buf,count);
    }else{
        console_put_str("sys_write:not allowed to write file\n");
        return -1;
    }
}
/**
 * @brief 
 * 功能:
 *  打开或创建文件
 * 参数:
 *  pathname:文件全路径
 *  flags:操作标识
 * 成功返回 pcb filetable 文件描述符
 * 失败返回 -1
 */
int32_t sys_open(const char* pathname,uint8_t flags){
    //sys_open只能打开文件
    if(pathname[strlen(pathname)-1] =='/'){
        dbg_printf("can't open directory %s\n",pathname);
        return -1;
    }
    
    int32_t fd = -1;

    path_seach_record search_record;
    memset(&search_record,0,sizeof(path_seach_record));

    //记录完整目录深度
    uint32_t pathname_depth = path_depth_cnt(pathname);

    //检查文件是否存在
    int32_t inode_no = search_file(pathname,&search_record);
    bool found = inode_no==-1?false:true;

    //如果是目录则退出
    if(search_record.file_type == FT_DIRECTORY){
        dbg_printf("sys_open can't open directory\n");
        dir_close(search_record.parent_dir);
        return -1;
    }

    //检查路径是否'断链'
    uint32_t path_search_depth = path_depth_cnt(search_record.searched_path);
    if(path_search_depth != pathname_depth){
        dbg_printf("can't open %s\n",search_record.searched_path);
        dir_close(search_record.parent_dir);
        return -1;
    }

    //如果路径全部存在
    
    //打开文件但文件不存在则返回失败
    if(!found && !(flags& O_CREAT)){
        dbg_printf("open file failed,%s does not exists\n",pathname);
        dir_close(search_record.parent_dir);
        return -1;
    }
    //只创建文件但是文件存在则返回失败
    else if(found && (flags==O_CREAT)){
        dbg_printf("create file failed,%s already exists\n",pathname);
        dir_close(search_record.parent_dir);
        return -1;
    }
    switch(flags&O_CREAT){
        //创建+读写
        case O_CREAT:
        {   
            dbg_printf("start to create file %s\n",strrchar(search_record.searched_path,'/')+1);
            if(!found){
                fd = file_create(search_record.parent_dir,strrchar(search_record.searched_path,'/')+1,flags);
                dir_close(search_record.parent_dir);
            }
            //再次检查文件是否存在
            int32_t i_no = search_file(pathname,&search_record);
            if(i_no !=-1 ){
                fd = file_open(i_no,flags);
            }
            dir_close(search_record.parent_dir);
            break;
        }
        //只读写
        default:
        {   
            //其他flags均为打开文件
            fd = file_open(inode_no,flags);
            break;
        }
        //打开文件
        dbg_printf("none\n");

    }
    
    return fd;

}
/**
 * @brief 
 * 文件描述符转换为全局文件表下标
 */
int32_t fd_idx2global_ftable_idx(uint32_t fd_idx){
    task_struct* cur = get_running_thread_pcb();
    int32_t g_idx = cur->fd_table[fd_idx];
    assert(g_idx>=0&&g_idx<MAX_FILE_OPEN);
    return g_idx;
}
/**
 * @brief 
 * 关闭文件
 */
bool sys_close(int32_t fd_idx){
    bool ret = false;
    if(fd_idx<2){
        //标准输入输出和错误输出不能关闭
        dbg_printf("can't close fd_idx <2\n");
        return false;
    }else{
        uint32_t global_fd = fd_idx2global_ftable_idx(fd_idx);
        //关闭管道的额外操作
        if(is_pipe(fd_idx)){
            file* f_pipe = &file_table[global_fd];
            //管道只被打开一次则释放管道缓冲区
            if(--f_pipe->fd_pos ==0){
                dbg_printf("global_fd:%d,pipe mfree buffer 0x%x\n",global_fd,f_pipe->fd_inode);
                mfree_page(PF_KERNEL,f_pipe->fd_inode,1);
                //这里置为null之后，全局文件表也就被释放了
                f_pipe->fd_inode =NULL;
            }
            ret=true;
        }else{
            //正常的文件关闭流程
            file_close(&file_table[global_fd]);
        }
        get_running_thread_pcb()->fd_table[fd_idx] =-1; //使文件描述符位可用
        ret=true;
    }
    return ret;
}
/**
 * @brief 
 * 读取文件
 * 成功，返回读取的字节数。失败,返回-1
 */
static int32_t file_read(file* file,void* buf,uint32_t count){
    uint8_t* buf_dst = (uint8_t*)buf;
    uint32_t size = count;

    uint32_t file_size = file->fd_inode->i_bsize;//文件大小

    if((file->fd_pos+ count) > file_size){
        //要读取的字节数，大于当前指针位置到文件末尾的字节数
        size = file_size - file->fd_pos;
        if(size == 0){
            //到文件尾
            return -1;
        }
    }
    uint32_t size_left = size; //剩余要读取的字节

    //dbg_printf("file_read: read file size:%d\n",size);
    //9.28 补丁，从sys_malloc换为使用内核缓冲区
    uint8_t* io_buf = get_kernel_pages(1);
    if(io_buf ==NULL){
        dbg_printf("file_read:sys_malloc error\n");
        return -1;
    }

    uint32_t* all_block = (uint32_t*)get_kernel_pages(1);

    if(all_block ==NULL){
        dbg_printf("file_read:sys_malloc error\n");
        sys_free(io_buf);
        return -1;
    }

    uint32_t block_read_start_idx = file->fd_pos /BLOCK_SIZE;//要读取数据的起始地址
    uint32_t block_read_end_idx = (file->fd_pos+ size)/BLOCK_SIZE; //要读取数据的结束位置

    uint32_t read_blocks = block_read_end_idx - block_read_start_idx;//要读取的总块数

    assert(block_read_start_idx <139&&block_read_end_idx<139);

    uint32_t indirect_block_table; //以及间接表地址
    uint32_t block_idx;           //获取待读取的块地址
    uint32_t* file_sectors = (uint32_t*)&file->fd_inode->i_sectors;

    //在同一块内读取数据
    if(read_blocks == 0)
    {   
        if(block_read_end_idx <12){
            //要读取的数据只在直接块
            block_idx = block_read_end_idx;
            all_block[block_idx] = file->fd_inode->i_sectors[block_idx];
        }else{
            //要读取的数据存在于间接块
            indirect_block_table = file_sectors[12];
            ide_read(cur_part->my_disk,indirect_block_table,all_block+12,1);
        }
    }
    //若要读取多个块
    else
    {   
        //要读取的块在直接块
        if(block_read_end_idx < 12){
            block_idx = block_read_start_idx;
            while (block_idx <= block_read_end_idx)
            {
                all_block[block_idx] = file_sectors[block_idx];
                block_idx++;
            }
            
        }
        //要读取的数块跨越直接快和间接块
        else if(block_read_start_idx<12&&block_read_end_idx>=12)
        {
            //先将直接块读入allblock
            block_idx = block_read_start_idx;
            while (block_idx < 12)
            {
                all_block[block_idx] = file_sectors[block_idx];
                block_idx++;
            }
            assert(file_sectors[12]!=0);
            indirect_block_table = file_sectors[12];
            ide_read(cur_part->my_disk,indirect_block_table,all_block+12,1);

        }
        //要读取的数据块只在间接块中
        else
        {
            assert(file_sectors[12]!=0);
            indirect_block_table = file_sectors[12];
            ide_read(cur_part->my_disk,indirect_block_table,all_block+12,1);
        }
    }

    //要读取的块地址已经全部复制到all_blocks中，下面开始遍历，读出数据

    uint32_t sec_idx,sec_lba,sec_off_bytes,sec_left_bytes,chunk_size;
    uint32_t bytes_read = 0; //已读取数据字节数
    while(bytes_read<size)
    {
        sec_idx = file->fd_pos /BLOCK_SIZE;
        sec_lba = all_block[sec_idx];
        sec_off_bytes = file->fd_pos % BLOCK_SIZE; 
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
        if(size_left<sec_left_bytes){
            chunk_size = size_left;
        }else{
            chunk_size = sec_left_bytes;
        }

        memset(io_buf,0,BLOCK_SIZE);

        ide_read(cur_part->my_disk,sec_lba,io_buf,1);
        memcpy(buf_dst,io_buf+sec_off_bytes,chunk_size);
        
        buf_dst += chunk_size;
        file->fd_pos +=chunk_size;
        bytes_read += chunk_size;
        size_left -=chunk_size;
    }
    mfree_page(PF_KERNEL,all_block,1);
    mfree_page(PF_KERNEL,io_buf,1);
    return bytes_read;
}
/**
 * @brief 
 * 从文件描述符fd中读取count个字节到buf,支持标准输入流
 * 成功返回读取的字节数，失败返回-1
 */
int32_t sys_read(int32_t fd,void* buf,uint32_t count){
    if(count==0){
        return 0;
    }
    int32_t ret = -1;
    if(fd<0){
        dbg_printf("sys_read: fd error\n");
        return -1;
    }
    assert(buf!=NULL);
    //管道
    if(is_pipe(fd)){
        return pipe_read(fd,buf,count);
    }
    if(fd ==stdin_no){
            char* buffer = buf;
            int32_t bytes_read = 0;
            while(bytes_read<(int32_t)count){
                *buffer = ioq_getchar(&keyboard_iobuffer);
                buffer++;
                bytes_read++;
            }
            ret = (bytes_read==0)?-1:bytes_read;
    }else{
        uint32_t _fd = fd_idx2global_ftable_idx(fd);
        ret = file_read(&file_table[_fd],buf,count);
    }
    return ret;

}
/**
 * @brief 
 * 设置文件指针位置
 * 成功返回新的文件指针偏移量，失败返回-1
 */
int32_t sys_lseek(int32_t fd,int32_t offset,uint8_t whence){
    if(fd< 0){
        dbg_printf("sys_lseek: fd invalid\n");
    }
    assert(whence>=SEEK_SET&&whence<=SEEK_END);
    uint32_t _fd = fd_idx2global_ftable_idx(fd);
    
    int32_t new_pos = 0; //新的偏移量
    file * pf = &file_table[_fd];
    int32_t file_size = pf->fd_inode->i_bsize;
    
    switch(whence){
        case SEEK_CUR:
        {   
            new_pos = (int32_t)pf->fd_pos+offset;
            break;
        }
        case SEEK_SET:
        {
            new_pos = offset;
            break;
        }
        case SEEK_END:
        {   
            new_pos = file_size + offset;
            break;
        }
    }
    if(new_pos<=0 || (new_pos>(file_size-1))){
        //读写位置必须位于文件范围内
        return -1;
    }
    pf->fd_pos = new_pos;
    return new_pos;
}
/**
 * @brief 
 * 删除文件
 */

/**
 * @brief 
 * 回收硬盘中的inode
 */
static void inode_delete(partition* part,uint32_t inode_no,void* io_buf){
    inode_position inode_pos;
    inode_locate(part,inode_no,&inode_pos);

    char* inode_buf = (char*)io_buf;
    
    int8_t block_read_cnt = inode_pos.two_sec?2:1;

    //清除inode
    ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,block_read_cnt);
    memset(inode_buf+inode_pos.off_size,0,sizeof(inode));
    ide_write(part->my_disk,inode_pos.sec_lba,inode_buf,block_read_cnt);
}
/**
 * @brief 
 * 回收inode的数据块和inode位图
 */
static void inode_release(partition* part,uint32_t inode_no){
    inode* inode_to_del = inode_open(part,inode_no);

    /*回收inode的所有数据块*/
    uint8_t block_idx  = 0,block_cnt = 12;
    uint32_t block_bitmap_idx;
    uint32_t all_blocks[140] = {0};

    //记录12个直接块地址
    do{
        all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
    }while(++block_idx<12);
    //间接块处理
    if(inode_to_del->i_sectors[12] != 0){
        block_cnt = 140;

        ide_read(part->my_disk,inode_to_del->i_sectors[12],all_blocks+12,1);
        block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
    
        assert(block_bitmap_idx > 0 );
        //回收一级间接表所在block块
        bitmap_set(&part->block_bitmap,block_bitmap_idx,0);
        bitmap_sync(part,block_bitmap_idx,BLOCK_BITMAP);
    }
    block_idx = 0;
    //回收12个直接块 block位图
    while(block_idx< block_cnt){
        if(all_blocks[block_idx] != 0){
            
            block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            assert(block_bitmap_idx > 0 );
            bitmap_set(&part->block_bitmap,block_bitmap_idx,0);
            bitmap_sync(part,block_bitmap_idx,BLOCK_BITMAP);
        }
        block_idx++;
    }
    //回收inode位图
    bitmap_set(&part->inode_bitmap,inode_no,0);
    bitmap_sync(part,inode_no,INODE_BITMAP);

    /*test 部分*/
    void* io_buf = sys_malloc(1024);
    inode_delete(part,inode_no,io_buf);
    sys_free(io_buf);


    inode_close(inode_to_del);
}
/**
 * @brief 
 * 删除文件(不能删除目录)
 * 成功返回true，失败返回false
 */
int32_t sys_unlink(const char* pathname){

    path_seach_record search_record;
    memset(&search_record,0,sizeof(path_seach_record));
    int32_t inode_no = search_file(pathname,&search_record);
    if(inode_no ==-1 ){
        dbg_printf("file %s not found \n",pathname);
        return false;
    }
    if(search_record.file_type ==FT_DIRECTORY){
        dbg_printf("can't delete a direcotry\n");
        return false;
    }
    /*检查文件是否在全局文件表中(是否在使用中)*/
    uint32_t file_idx = 0;
    while (file_idx<MAX_FILE_OPEN)
    {
        if((file_table[file_idx].fd_inode!=NULL )&&
        (int32_t)file_table[file_idx].fd_inode->i_no == inode_no){
            break;
        }
        file_idx++;
    }
    
    if(file_idx<MAX_FILE_OPEN){
        dir_close(search_record.parent_dir);
        dbg_printf("file %s is in use,not allow to delete!\n",pathname);
        return false;
    }
    assert(MAX_FILE_OPEN==file_idx);

    void* io_buf = sys_malloc(SECTOR_SIZE*2);
    if(io_buf == NULL){
        dir_close(search_record.parent_dir);
        dbg_printf("sys_unlink:malloc error\n");
        return false;
    }

    dir* parent_dir = search_record.parent_dir;

    bool flag = delete_dir_entry(cur_part,parent_dir,inode_no,io_buf);
    if(!flag){
        dir_close(search_record.parent_dir);
        sys_free(io_buf);
        dbg_printf("delete_dir_entry return error\n");
        return false;
    }
    inode_release(cur_part,inode_no);
    sys_free(io_buf);
    dir_close(search_record.parent_dir);\
    return true;
}

/**
 * @brief 
 * 创建目录 pathname
 * 成功返回 true,失败返回false
 */
bool sys_mkdir(const char* pathname){

    dbg_printf("*********sys_mkdir*********\n");
    //申请缓冲区
    uint8_t rollback_step = 0;
    void* io_buf = sys_malloc(SECTOR_SIZE*2);
    if(io_buf ==NULL){
        dbg_printf("sys_mkdir:malloc error\n");
        return false;
    }

    //  1.搜索目录是否已存在
    path_seach_record searched_record; //路径搜索结构体
    memset(&searched_record,0,sizeof(path_seach_record));
    int32_t inode_no = -1;
    inode_no = search_file(pathname,&searched_record);
    if(inode_no != -1){
        dbg_printf("sys_mkdir:file or directory %s exist\n",pathname);
        rollback_step =1;
        goto rollback;
    }else
    {
        uint32_t pathname_depth = path_depth_cnt(pathname);
        uint32_t path_serch_depth = path_depth_cnt(searched_record.searched_path);
        if(pathname_depth != path_serch_depth){
            dbg_printf("sys_mkdir: cannot access %s:Not a directory,subpath %s is't exist\n",pathname,searched_record.searched_path);
            rollback_step =1;
            goto rollback;
        }
    }

    //  2.为目录创建inode
    dir* parent_dir = searched_record.parent_dir;
    char* dirname  = strrchar(searched_record.searched_path,'/')+1;
    inode new_inode;
    inode_no = inode_create(&new_inode);  
    if(!inode_no){
        dbg_printf("sys_mkdir:inode_create error\n");
        rollback_step =1;
        goto rollback; 
    }
    //还原步骤2  inode_create  io_buf
    
    // 3.为新目录创建一个直接块存储目录项
    uint32_t block_bitmap_idx = 0;
    int32_t block_lba = -1;

    dbg_printf("NO sync memory block bitmap value:%x\n",*cur_part->block_bitmap.bits);
    block_lba = block_bitmap_alloc(cur_part);

    dbg_printf("block idx:0x%x\n",block_lba-cur_part->sb->data_start_lba);
    if(block_lba==-1){
        dbg_printf("sys_mkdir:block_bitmap_alloc error\n");
        rollback_step =2;
        goto rollback; 
    }
    new_inode.i_sectors[0] = block_lba;
    block_bitmap_idx = block_lba-cur_part->sb->data_start_lba;
    assert(block_bitmap_idx!=0);

    bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

    //还原步骤 3  inode_create  io_buf 同步到硬盘的block位图信息



    //  4.为新目录项写入'.'和'..'
    memset(io_buf,0,SECTOR_SIZE*2);
    dir_entry* p_de = (dir_entry*)io_buf;
    memcpy(p_de->filename,".",1);
    p_de->f_type = FT_DIRECTORY;
    p_de->i_no = inode_no;

    p_de++;
    memcpy(p_de->filename,"..",2);
    p_de->f_type = FT_DIRECTORY;
    p_de->i_no = parent_dir->inode->i_no;

    ide_write(cur_part->my_disk,new_inode.i_sectors[0],io_buf,1);

    //这里忘记修改inode的size，导致遍历函数无法完全遍历目录
    new_inode.i_bsize = 2*cur_part->sb->dir_entry_size;
    // 5.在父目录项中添加新创建的目录项
    dir_entry new_dir_entry;
    memset(&new_dir_entry,0,sizeof(dir_entry));
    create_dir_entry(dirname,new_inode.i_no,FT_DIRECTORY,&new_dir_entry);
    memset(io_buf,0,SECTOR_SIZE*2);
    if(!sync_dir_entry(parent_dir,&new_dir_entry,io_buf)){
        dbg_printf("sys_mkdir:sync_dir_entry error");
        rollback_step =3;
        goto rollback; 
    }

    // 6.同步
    memset(io_buf,0,SECTOR_SIZE*2);
    inode_sync(cur_part,parent_dir->inode,io_buf);



    memset(io_buf,0,SECTOR_SIZE*2);
    inode_sync(cur_part,&new_inode,io_buf);

    bitmap_sync(cur_part,inode_no,INODE_BITMAP);

    dir_close(searched_record.parent_dir);
    dbg_printf("make dir ok: dir inofo:\ndir_name:%s,dir_isize:%d,dir_no:%d\n",pathname,new_inode.i_bsize,new_inode.i_no);
    dbg_printf("*********sys_end*********\n");
    return true;
 
rollback:
    switch (rollback_step)
    {   
        case 3:
            bitmap_set(&cur_part->block_bitmap,block_bitmap_idx,0);
            bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
        case 2:
            bitmap_set(&cur_part->inode_bitmap,inode_no,0);
        case 1:
            dir_close(searched_record.parent_dir);
    }
    sys_free(io_buf);
    return false;
}
/**
 * @brief 
 * 打开目录
 * 成功返回目录指针，失败返回NULL 
 */
dir* sys_opendir(const char* name){
    assert(strlen(name)< MAX_PATH_LEN);
    dir* ret =NULL;
    if((strcmp(name,"/")==0) || (strcmp(name,"/.")==0) || (strcmp(name,"/..")==0)){
        return &root_dir;
    }
    //检查目录是否存在
    path_seach_record searched_record;
    memset(&searched_record,0,sizeof(path_seach_record));
    int inode_no = search_file(name,&searched_record);
    if(inode_no == -1){
        dbg_printf("In %s ,sub path %s not exist \n",name,searched_record.searched_path);
    }else if(searched_record.file_type == FT_REGULAR){
        dbg_printf("%s is regular file!\n",name);
    }else if(searched_record.file_type == FT_DIRECTORY){
        ret = dir_open(cur_part,inode_no);
    }
    
    if(searched_record.parent_dir){
        dir_close(searched_record.parent_dir);
    }
    return ret;

}
/**
 * @brief 
 * 关闭目录
 * 成功返回true,失败返回false
 */
bool sys_closedir(dir* dir){
    bool ret = -1;
    if(dir!=NULL){
        dir_close(dir);
        ret = true;
    }
    return ret;
}
/**
 * @brief 
 * 读取目录dir的一个目录项,更新dir_pos的位置
 * 成功返回其目录项地址
 * 失败返回NUL
 */
dir_entry* sys_readdir(dir* dir){
    assert(dir != NULL)
    return dir_read(dir);
}
/**
 * @brief 
 * 把目录dir的dir_pos置零
 */
void sys_rewinddir(dir* dir){
    dir->dir_pos =0;
}

/**
 * @brief 
 * 判断目录是否为空
 * 为空返回true，不为空返回false
 */
static bool dir_is_empty(dir* dir){

    inode* dir_inode = dir->inode;
    //通过判断目录大小，判断目录是否为空
    return (dir_inode->i_bsize == cur_part->sb->dir_entry_size*2);
}
/**
 * @brief 
 * 在父目录 parent_dir 中删除 child_dir
 * 成功返回true,失败返回false
 */
static bool dir_remove(dir* parent_dir,dir* child_dir){
    inode* child_inode = child_dir->inode;
    int32_t block_idx = 1;
    while (block_idx<13)
    {
        if(child_inode->i_sectors[block_idx] != 0){
            dbg_printf("remov fail,the directory is not empty\n");
            return false;
        }
        block_idx++;
    }
    void* io_buf = sys_malloc(SECTOR_SIZE*2);
    if(io_buf == NULL){
        dbg_printf("dir_remove: malloc for io_buf failed\n");
        return false;
    }

    delete_dir_entry(cur_part,parent_dir,child_inode->i_no,io_buf);

    inode_release(cur_part,child_inode->i_no);
    sys_free(io_buf);
    return true;
}

/**
 * @brief 
 * 删除空目录
 * 成功时返回true，失败时返回false
 */
bool sys_rmdir(const char* pathname){
    path_seach_record searched_record;
    memset(&searched_record,0,sizeof(path_seach_record));
    int inode_no = search_file(pathname,&searched_record);
    bool state =  false;
    if(inode_no == -1){
        dbg_printf("In %s,sub path %s not exist\n",searched_record.searched_path);
    }else{
        if(searched_record.file_type == FT_REGULAR){
            dbg_printf("%s is regular file\n",searched_record.searched_path);
        }else{
            dir* dir = dir_open(cur_part,inode_no);
            if(!dir_is_empty(dir)){
                dbg_printf("dir %s is not empty,it is not allowed to delete a nonempty directory\n",pathname);
            }else{
                if(dir_remove(searched_record.parent_dir,dir)){
                    state =true;
                }
            }
            dir_close(dir);
        }
    }
    dir_close(searched_record.parent_dir);
    return state;
}

/**
 * @brief 
 * 获得父目录的inode编号
 * 成功返回其父目录inode编号
 * 没有失败
 */
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr,void* io_buf){
    //通过获取子inode的直接数据块中的 ".."inode,返回其inode编号
    inode* child_dir_inode = inode_open(cur_part,child_inode_nr);

    uint32_t block_lba  = child_dir_inode->i_sectors[0];
    assert(block_lba!=0);
    ide_read(cur_part->my_disk,block_lba,io_buf,1);

    dir_entry* dir_e = (dir_entry*)io_buf;
    inode_close(child_dir_inode);
    return dir_e[1].i_no;
}
/**
 * @brief 
 * 在inode编号为 p_inode_nr 的目录中查找
 * inode编号为c_inode_nr的文件或文件夹的名字
 * 将名字存入缓冲区path
 * 成功返回true,失败返回false
 */
static bool get_child_dir_name(uint32_t p_inode_nr,uint32_t c_inode_nr,char* path,void* io_buf){
   
    //复制数据块到栈
    inode* parent_dir_inode = inode_open(cur_part,p_inode_nr);
    uint32_t all_blocks[140] = {0};
    copy_inode_isectors(cur_part->my_disk,parent_dir_inode,all_blocks);
    inode_close(parent_dir_inode);

    //遍历所有块
    uint32_t block_idx = 0,block_cnt = 12;
    if(all_blocks[12]!=0)block_cnt = 140;
    uint32_t dir_sec = BLOCK_SIZE/cur_part->sb->dir_entry_size;
    uint32_t dir_entry_idx = 0;
    while(block_idx< block_cnt){
        if(all_blocks[block_idx]!= 0)
        {
            ide_read(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
            dir_entry* dir_e = (dir_entry*)io_buf;
            while(dir_entry_idx<dir_sec){
                
                if((dir_e+dir_entry_idx)->i_no == c_inode_nr)
                {
                    strcat(path,"/");
                    strcat(path,(dir_e+dir_entry_idx)->filename);
                    return true;
                }
                dir_entry_idx++;
            }
        }
        block_idx++;
    }
    return false;

}

/**
 * @brief 
 * 把当前工作目录绝对路径写入到buf,size是buf的大小。
 * 当buf为NULL时,由操作系统分配存储工作路径的空间。
 * 成功,返回字符串返回地址。失败返回NULL
 */

char* sys_getcwd(char* buf,uint32_t size){
    
    assert(buf!=NULL);
    void* io_buf = sys_malloc(SECTOR_SIZE);
    if(io_buf == NULL){
        return NULL;
    }
    task_struct* cur_thread = get_running_thread_pcb();
    int32_t parent_inode_nr = 0;
    int32_t child_inode_nr = cur_thread->cwd_inode_nr;
    bool state = false;
    assert(child_inode_nr>=0 && child_inode_nr<=4096);

    if(child_inode_nr == 0){
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    memset(buf,0,size);
    char full_path_reverse[MAX_PATH_LEN] = {0};

    while(child_inode_nr){
        parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr,io_buf);
        state = get_child_dir_name(parent_inode_nr,child_inode_nr,full_path_reverse,io_buf);
        if(!state){
            sys_free(io_buf);
            return NULL;
        }
        child_inode_nr = parent_inode_nr;
    }

    char* last_slash;
    //反转路径，就很巧妙
    while((last_slash = strrchar(full_path_reverse,'/'))){
        uint16_t len = strlen(buf);
        strcpy(buf+len,last_slash);
        *last_slash = 0;
    }
    sys_free(io_buf);
    return buf;
}

/**
 * @brief 
 * 更改当前工作目录为path(绝对路径)
 * 成功返回true,失败返回false
 */
bool sys_chdir(const char* path){
    bool ret = false;

    path_seach_record searched_record;
    memset(&searched_record,0,sizeof(path_seach_record));
    int32_t inode_no = search_file(path,&searched_record);
    if(inode_no != -1){
        if(searched_record.file_type == FT_DIRECTORY){
            get_running_thread_pcb()->cwd_inode_nr = inode_no;
            ret = true;
        }else{
            dbg_printf("sys_chdir: %s is regular file or other!\n",path);
        }
    }else{
        dbg_printf("sys_chdir: error path no exist\n");
    }
    return ret;
}
/**
 * @brief 
 * 在buf中填充文件结构相关信息。
 * 成功返回true.失败返回false
 */
bool sys_stat(const char* path,struct _stat* buf){

    if(!strcmp(path,"/") || !strcmp(path,"/.") || !strcmp(path,"/..")){
        buf->st_file_type = FT_DIRECTORY;
        buf->st_ino =  0;
        buf->st_size = root_dir.inode->i_bsize;
        return true;
    }

    bool state = false;

    path_seach_record searched_record;
    memset(&searched_record,0,sizeof(path_seach_record));
    int32_t inode_no =  search_file(path,&searched_record);

    if(inode_no != -1){
        inode* inode = inode_open(cur_part,inode_no);
        buf->st_file_type = searched_record.file_type;
        buf->st_ino = inode->i_no;
        buf->st_size = inode->i_bsize;
        state = true;
    }else{
        dbg_printf("sys_stat: error! %s file no exist\n",searched_record.searched_path);
        state =false;
    }
    return state;
}
void sys_help(void){
    printf("buildin commands:\n\
    ls: Displays files and folders in the current directory\n\
    cd: Changing the current directory\n\
    mkdir:Create a directory\n\
    rmdir:Remove a directory\n\
    pwd:Display current directory\n\
    ps:Displaying Process Information\n\
    clear:Clear the screen\n\
    exit:Exit the Shell\n\
shortcuts key:\n\
    ctrl+l:Clear the screen\n\
    ctrl+u:Clear input\n\
hint:\n\
    Commands and arguments are separated by Spaces\n\
    Pipe separator for \"|\"\n");
}

/**
 * @brief 
 * 输出文件系统属性值
 */
void show_fs(void){
    dbg_printf("cur_part start_lba:%d\n",cur_part->sb->data_start_lba);
    dbg_printf("cur_thread file_table virtual address:0x%x\n",get_running_thread_pcb()->fd_table);
    dbg_printf("global file table virtual address:0x%x\n",file_table);
    //  asm volatile ("movl %0,%%cr3" : : "r"(pagedir_phy_addr) : "memory");
    dbg_printf("EIP:0x%x\n",get_eip());
}
/**
 * @brief 
 * 初始化文件系统
 */
void filesys_init(void){
    //搜索磁盘文件系统
    search_filesys();
    //默认操作分区
    char default_part[8] = "sdb1";
    //挂载分区
    list_traversal(&partition_list,mount_partition,default_part);
    dbg_printf("block bitmap value:%x\n",*cur_part->block_bitmap.bits);
    //打开当前分区分区的根目录
    open_root_dir(cur_part);

    //初始化全局文件表
    memset(file_table,0,sizeof(file_table));
    dbg_printf("block bitmap value:%x\n",*cur_part->block_bitmap.bits);
}
