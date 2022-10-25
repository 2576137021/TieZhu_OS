#ifndef _FS_SUPER_BLOCK_H
#define _FS_SUPER_BLOCK_H
#include "../lib/kernel/stdint.h"
struct _super_block{ //存放在每个分区的第二个扇区，大小  = 1扇区.

    uint32_t magic;         //魔数,标识文件系统类型

    uint32_t sec_cnt;       //本分区中的扇区数量
    uint32_t inode_cnt;     //本分区中inode数量
    uint32_t part_lba_base; //本分区的起始lba地址

    uint32_t block_bitmap_lba; //空闲块位图起始扇区地址
    uint32_t block_bitmap_sects;//空闲块位图所占的扇区数量

    uint32_t inode_bitmap_lba;  //inode位图地址
    uint32_t inode_bitmap_sects;//inode位图占用的扇区数量

    uint32_t inode_table_lba;   //inode table 的地址
    uint32_t inode_table_sects; //inode table占用的扇区大小

    
    uint32_t root_inode_no; //根目录所在的Inode节点号
    uint32_t dir_entry_size;//目录项大小

    uint32_t data_start_lba; //空闲块起始扇区地址

    uint8_t pad[460];   //凑够一扇区大小
}__attribute__((packed));
typedef struct _super_block super_block;
#endif