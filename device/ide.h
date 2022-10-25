#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/bitmap.h"
#include "../thread/sync.h"
/*分区表结构*/
typedef struct _partition
{
    uint32_t start_lba; //起始扇区
    uint32_t sec_cnt; //扇区数
    struct _disk* my_disk;      //分区所属的硬盘
    list_elem part_tag; //队列中的标记
    char name[8];       //分区名称
    struct _super_block* sb;    //本分区的超级块
    BitMap block_bitmap;//块位图
    BitMap inode_bitmap;//i结点位图
    list open_inodes;   //本分区打开的i节点队列
}partition;
/*硬盘结构*/
typedef struct _disk
{
    char name[8];       //本硬盘的名称
    struct _ide_channel* my_channel;    //此块硬盘属于哪个ide通道
    uint8_t dev_no;//本硬盘是主盘(0)还是从盘(1)
    partition prim_parts[4];//一个硬盘只有最多四个主分区
    partition logic_parts[8];//逻辑分区,我们设置为最多8个，理论上无限多
}disk;

/*ata通道结构*/
typedef struct _ide_channel
{
    char name[8];//ata通道名称
    uint16_t port_base; //本通道的起始端口号
    uint8_t irq_no; //本通道的所用的中断号
    lock lock;//通道锁
    bool expecting_intr;//true表示期望获得硬盘的中断
    semaphore disk_done;// 用于阻塞和唤醒驱动程序
    disk devices[2]; //一个通道上连接两个硬盘，一主一从
}ide_channel;

ide_channel channels[2];    //两个IDE通道
uint16_t channel_cnt;       //ide的通道数量,一个通道上有两个硬盘
list partition_list;        //所有分区链表


/**
 * @brief 
 * 从硬盘(hd)读取(sec_cnt)个扇区到缓冲区(buf)
 * 没有发生错误返回true,读取错误返回false
 */
bool ide_read(disk* hd,uint32_t lba,void* buf, uint32_t sec_cnt);
/**
 * @brief 
 * 将buf中的sec_cnt个扇区大小(512 byte)的数据写入到磁盘
 */
void ide_write(disk* hd,uint32_t lba,void* buf, uint32_t sec_cnt);
/**
 * @brief 
 * 硬盘中断处理程序
 */
void ide_hd_handler(uint8_t irq_no);
/**
 * @brief 
 * 硬盘数据结构初始化
 */
void ide_init(void);


#endif