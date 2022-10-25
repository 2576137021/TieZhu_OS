#include "dir.h"
#include "inode.h"
#include "super_block.h"
#include "fs.h"
#include "file.h"
#include "../device/ide.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/stdio-kernel.h"

dir root_dir;
/**
 * @brief 
 * 打开根目录
 */
void open_root_dir(partition* part){
    root_dir.inode = inode_open(part,part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}
/**
 * @brief 
 * 在分区part上打开i节点为inode_no的目录并返回目录指针
 */
dir* dir_open(partition* part,uint32_t inode_no){

    dir* pdir = (dir*)sys_malloc(sizeof(dir));
    pdir->dir_pos = 0;
    pdir->inode = inode_open(part,inode_no);
    return pdir;
}
/**
 * @brief 
 * 在part分区pdir目录搜索名为name的目录或文件
 * 成功返回true，目录项存入dir_e
 * 失败返回false
 */
bool search_dir_entry(partition* part,dir* pdir,const char* name,dir_entry* dir_e){
    uint32_t block_cnt = 12+128; //基本块数量

    //复制所有块内容
    uint32_t* all_blocks = (uint32_t*)sys_malloc(4*block_cnt);
    uint32_t block_idx = 0;
    while (block_idx<12)
    {
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
        block_idx++;
    }
    block_idx = 0;
    if(all_blocks[12]!=0){//有一级间接表 一级间接表的大小为 128*4 = 512 byte
        ide_read(part->my_disk,pdir->inode->i_sectors[12],all_blocks+12,1);
    }
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
    dir_entry* p_de = (dir_entry*) buf;

    
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE/dir_entry_size;//一个扇区可容纳的目录项个数

    uint32_t dir_entry_idx = 0;
    /*在所有块中查找目录项*/
    while(block_idx<block_cnt){ 
         //遍历所有的块,每一个块都是一个扇区

        if(all_blocks[block_idx]==0){//如果基本块地址为空跳过循环
            block_idx++;
            continue;
        }

        ide_read(part->my_disk,all_blocks[block_idx],p_de,1);//通过直接块读取硬盘数据到缓冲区

        uint32_t dir_entry_idx = 0;
        while(dir_entry_idx<dir_entry_cnt){ //遍历这一个块(扇区)中的所有目录项
            if(!strcmp(p_de->filename,(char*)name)){
                memcpy(dir_e,p_de,dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks);
                return true;
            }
            dir_entry_idx++; 
            p_de++;   //目录项++
        }
        block_idx++; //块++
        p_de = (dir_entry*)buf;
        memset(buf,0,SECTOR_SIZE);
        
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

/**
 * @brief 
 * 关闭目录
 */
void dir_close(dir* dir){
    /*不能关闭根目录*/
    if(dir == &root_dir){
        return ;
    }
    inode_close(dir->inode);
    sys_free(dir);
}

/**
 * @brief 
 * 在内存中初始化目录项p_de
 */
void create_dir_entry(char* filename,uint32_t inode_no,uint8_t file_type,\
dir_entry* p_de){
    assert(strlen(filename)<MAX_FILE_NAME_LEN);

    p_de->f_type = file_type;
    p_de->i_no = inode_no;
    strcpy(p_de->filename,filename);
}

/**
 * @brief 
 * 将目录项 p_de写入父目录 parent_dir中,同步内存磁盘目录项数据到磁盘
 * io_buf::调用者提供的缓冲区,至少一个扇区大小
 * 此操作会改变 parent_dir 的inode属性值
 */
bool sync_dir_entry(dir* parent_dir,dir_entry* p_de,\
void* io_buf){
    inode* dir_inode = parent_dir->inode; // 父目录inode
    uint32_t dir_size = dir_inode->i_bsize; //父目录大小
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size; //目录项大小

    assert(dir_size%dir_entry_size==0) //两者成倍数

    uint32_t sec_dir_entry_cnt = 512/ dir_entry_size; //一个扇区中最多有几个目录项

    int32_t block_lba = -1;
    uint8_t block_idx = 0;
    uint32_t all_blocks[140]= {0};

    //将目录inode的12个直接数据块lba地址读入内存
    while (block_idx<12)
    {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }

    dir_entry* dir_e = (dir_entry*)io_buf;
    int32_t block_bitmap_idx = -1;
    block_idx = 0; //重置

    while (block_idx<(128+12)) //遍历目录的所有块,寻找一个空闲块
    {
        block_bitmap_idx = -1;

        //如果此块还未分配地址
        if(all_blocks[block_idx] ==0){ 
            
            block_lba = block_bitmap_alloc(cur_part);//在磁盘中申请一个空闲扇区
            if(block_lba == -1){//分配扇区失败
                dbg_printf("sync_dir_entry block_bitmap_alloc failed\n");
                return false;
            }
            //同步内存中的分区block位图到硬盘中
            block_bitmap_idx = block_lba-cur_part->sb->data_start_lba;
            assert(block_bitmap_idx!=-1);
            bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

            block_bitmap_idx = -1;//重置

            if(block_idx<12){
                //如果是未分配地址的直接块,同步
                dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
                //为什么这里不把更新后的目录直接块写入到磁盘呢?
              
            }else if(block_idx ==12){
                //如果是未分配地址的间接块
                //将申请的地址作为一级间接块的lba地址
                dir_inode->i_sectors[12] = block_lba;
                block_lba = -1;
                block_lba = block_bitmap_alloc(cur_part);//再申请一块空闲扇区
                if(block_lba==-1){
                    //块申请失败
                    //还原block位图和间接块地址
                    block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    bitmap_set(&cur_part->block_bitmap,block_bitmap_idx,0);
                    dir_inode->i_sectors[12] = 0;
                    dbg_printf("i_sectors[12],block_bitmap_alloc failed\n");
                    return false;
                }
                //块申请成功，同步内存位图到磁盘
                block_bitmap_idx = block_lba-cur_part->sb->data_start_lba;
                assert(block_bitmap_idx!=-1);
                bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
                //将间接块0的地址保存到 块数组中
                all_blocks[12] = block_lba;
                //将申请的扇区地址写入到磁盘中的一节间接表的第一项
                ide_write(cur_part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
            }else{ //>12的间接块
                all_blocks[block_idx] = block_lba;
                //将后面512字节的all_blocks全部同步到磁盘
                ide_write(cur_part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
            }
            //同步目录项到目录磁盘
            memset(io_buf,0,512);
            memcpy(io_buf,p_de,dir_entry_size);
            //因为是刚分配的地址所以不用读出再写入
            ide_write(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
            //改变当前目录的大小
            dir_inode->i_bsize +=dir_entry_size;
            //这里没有同步目录状态，在以后同步？
            return true;
        }
        //如果此块已分配地址,那么直接遍历这个扇区,查找闲置项
        ide_read(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
        uint8_t dir_entry_idx = 0;
        while (dir_entry_idx<sec_dir_entry_cnt)
        {
            if((dir_e+dir_entry_idx)->f_type==FT_UNKNOW){
                //已找到空闲目录项
                memcpy(dir_e+dir_entry_idx,p_de,dir_entry_size);
                dbg_printf("sync_dir_entry lba:0x%x\n",all_blocks[block_idx]);
                ide_write(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
                dir_inode->i_bsize +=dir_entry_size;
                return true;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    //遍历完目录所有数据块后仍未找到
    dbg_printf("directory is full\n");
    return  false;
}

/**
 * @brief 
 * 回收pdir目录中inode编号为inode_no的目录项
 */
bool delete_dir_entry(partition* part,dir* pdir,uint32_t inode_no,void* io_buf){
    
    inode* dir_inode = pdir->inode;
    uint32_t block_idx = 0, all_blocks[140] = {0};

    copy_inode_isectors(part->my_disk,dir_inode,all_blocks);

    /*目录项在存储时,保证了不会跨扇区存储*/
    uint32_t dir_entry_size = sizeof(dir_entry); //目录项大小
    uint32_t dir_entry_per_sec = SECTOR_SIZE/dir_entry_size; //一个扇区最多容纳的目录项

    dir_entry* dir_e = (dir_entry*)io_buf;
    dir_entry* dir_entry_found = NULL;
    uint8_t dir_entry_idx,dir_entry_cnt;
    bool is_dir_first_block = false; //目录的第一个块

    /*在pdir寻找目录项*/
    while(block_idx <140){
        is_dir_first_block = false;
        if(all_blocks[block_idx] == 0){
            block_idx++;
            continue;
        }
        dir_entry_idx = dir_entry_cnt = 0;
        dir_e = (dir_entry*)io_buf;
        memset(io_buf,0,SECTOR_SIZE);

        //读取数据块
        // dbg_printf("read lba:%x\n dir_entry size:%d",all_blocks[block_idx],dir_entry_size);
        ide_read(part->my_disk,all_blocks[block_idx],io_buf,1);

        /*遍历数据块中所有目录项*/
        while (dir_entry_idx < dir_entry_per_sec)
        {   
            // dbg_printf("dir_e ptr:0x%x\n",dir_e);
            // dbg_printf("file type:%d\n",dir_e->f_type);
            // dbg_printf("file name:%s\n",dir_e->filename);
            
            if((dir_e!=NULL) && dir_e->f_type !=FT_UNKNOW )
            {   

                if(!strcmp(dir_e->filename,"."))
                {
                    is_dir_first_block = true;
                }else if(strcmp(dir_e->filename,".")&& strcmp(dir_e->filename,".."))
                {   
                    //统计除根目录和上级目录的所有目录项数量
                    dir_entry_cnt++;
                    if(dir_e->i_no ==inode_no)
                    {   
                        //找到目录项
                        dir_entry_found = dir_e;
                    }
                }
            }
            dir_entry_idx++;
            dir_e =dir_e + 1;
        }

        //如果没找到目录项，则去下一个数据块寻找
        if(dir_entry_found == NULL){
            block_idx++;
            continue;
        }
        assert(dir_entry_cnt >=1);
        
        //找到目录项后,清除此目录项位图，并判断此数据块中是否只有该目录项。如果是则回收此数据块
        if(dir_entry_cnt ==1 && !is_dir_first_block)
        {
            //清除目录项，数据块位图
            uint32_t block_bitmap_idx = all_blocks[block_idx]- part->sb->data_start_lba;
            bitmap_set(&part->block_bitmap,block_bitmap_idx,0);
            bitmap_sync(part,block_bitmap_idx,BLOCK_BITMAP);

            //回收inode中的数据块
            if(block_idx < 12)
            {
                dir_inode->i_sectors[block_idx] = 0;
            }
            else
            {
                uint32_t indirect_blocks = 0;
                uint32_t indirect_block_idx = 12;
                while(indirect_block_idx < 140)
                {
                    if(all_blocks[indirect_block_idx] != 0)
                    { 
                        indirect_blocks++;
                    }
                    indirect_block_idx++;
                }
                assert(indirect_blocks >= 1);
                
                if(indirect_blocks == 1)
                {
                    //间接块中只有一个数据块

                    //回收一级间接表所在的Block位图
                    //注意此时硬盘的中目录数据并没有被清除
                    block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
                    bitmap_set(&part->block_bitmap,block_bitmap_idx,0);
                    bitmap_sync(part,block_bitmap_idx,BLOCK_BITMAP);
                    dir_inode->i_sectors[12] = 0;
                    
                }else
                {   
                    //如果间接块中有多个数据块
                    all_blocks[block_idx] = 0;
                    ide_write(part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
                }
            }
        }
        else
        {
            //只清除目录项
            memset(dir_entry_found,0,dir_entry_size);
            //同步更新后的inode数据块信息到硬盘
            ide_write(part->my_disk,all_blocks[block_idx],io_buf,1);
        }
        
        //同步目录inode状态
        assert(dir_inode->i_bsize >= dir_entry_size);
        dir_inode->i_bsize -=dir_entry_size;
        memset(io_buf,0,SECTOR_SIZE*2);
        inode_sync(part,dir_inode,io_buf);
        return true;
    }
    dbg_printf("Directory not found \n");
    return false;
}
/**
 * @brief 
 * 读取目录中的所有目录项(包括文件和目录)
 * 成功返回一个目录项，失败返回NULL
 */
dir_entry* dir_read(dir* dir){
    dir_entry* dir_e = (dir_entry*)dir->dir_buf;
    inode* dir_inode = dir->inode;
    
    uint32_t block_idx =0,dir_entry_idx = 0;
    uint32_t all_blocks[140] = {0};

    copy_inode_isectors(cur_part->my_disk,dir_inode,all_blocks);

    uint32_t cur_dir_entry_pos = 0;

    uint32_t dir_entry_size =cur_part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec = SECTOR_SIZE/dir_entry_size;

    while(dir->dir_pos < dir_inode->i_bsize){
        if(all_blocks[block_idx] == 0){
            block_idx++;
            continue;
        }
        //复制一个块中数据到缓冲区
        memset(dir_e,0,SECTOR_SIZE);
        ide_read(cur_part->my_disk,all_blocks[block_idx],dir_e,1);
        dir_entry_idx = 0;
        
        //遍历缓冲区中的所有目录项
        while(dir_entry_idx < dir_entrys_per_sec){
            if((dir_e+dir_entry_idx)->f_type!=FT_UNKNOW)
            {   
                //判断是否是已返回过的目录项
                if(cur_dir_entry_pos < dir->dir_pos){
                    cur_dir_entry_pos+= dir_entry_size;
                    dir_entry_idx++;
                    continue;;
                }
                assert(cur_dir_entry_pos == dir->dir_pos);
                dir->dir_pos += dir_entry_size;
                return dir_e+dir_entry_idx;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    return NULL;
}