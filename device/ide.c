#include "ide.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/string.h"
#include "../lib/stdio.h"
#include "../thread/thread.h"
#include "timer.h"
/**
 * @brief 
 * 定义硬盘命令块端口号
 * channel 通道(ide_channel)
 */
#define reg_data(channel) (channel->port_base)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2) //要读写的扇区数
#define reg_lba_l(channel) (channel->port_base + 3)
#define reg_lba_m(channel) (channel->port_base + 4)
#define reg_lba_h(channel) (channel->port_base + 5)
#define reg_device(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
/**
 * @brief 
 * 硬盘控制块寄存器
 * 详情参考笔记"硬盘"部分
 */
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_device_ctl(channel) (reg_alt_status(channel))
/**
 * @brief 
 * 定义硬盘alternate status 状态位
 */
#define BIT_ALT_STAT_BSY 0X80 //硬盘忙
#define BIT_ALT_STAT_DRDY 0x40 //驱动器准备好了
#define BIT_ALT_STAT_DRQ 0x8 //数据传输准备好了
/**
 * @brief 
 * devuce寄存器关键位
 */
#define BIT_DEV_MBS 0XA0 //固定位为1
#define BIT_DEV_MOD_LBA 0X40 //LBA寻址模式
#define BIT_DEV_DEV 0X10 //从盘
/**
 * @brief 
 * 硬盘操作命令
 */
#define CMD_IDENTIFY 0XEC //identify 获取硬盘信息，返回结果以字为单位
#define CMD_READ_SECTOR 0X20 //读扇区
#define CMD_WRITE_SECTOR 0x30 //写扇区

#define max_lba ((80*1024*1024/512)-1) //80MB磁盘的最大扇区数 lba是以0开始的扇区下标

//分区表项
struct _partition_table_entry{
    uint8_t boottable;  //是否可引导
    uint8_t start_head; //起始磁头号
    uint8_t start_sec;  //起始扇区号
    uint8_t start_chs;  //起始柱面号
    uint8_t fs_type;    //分区类型
    uint8_t end_head;   //结束磁头号
    uint8_t end_sec;    //结束扇区号
    uint8_t end_chs;    //结束柱面号

    uint32_t start_lba;  //本分区起始扇区的lba地址
    uint32_t sec_cnt;    //本分区的扇区数目
}__attribute__ ((packed));//禁止gcc对结构体进行填充,保证结构体大小为16字节
typedef struct _partition_table_entry partition_table_entry;
//引导扇区,mbr或ebr所在扇区
struct _boot_sector
{
    uint8_t other[446];	  			//446 + 64 + 2 446是拿来占位置的
    partition_table_entry partition_table[4];  //分区表中4项 64字节
    uint16_t signature;				//最后的标识标志 魔数0x55 0xaa				
}__attribute__ ((packed));
typedef struct _boot_sector boot_sector;

int32_t ext_lba_base =0;    //用于记录总扩展分区的起始lba,初始为0
uint8_t p_no = 0,l_no = 0;   //用于记录硬盘分区和逻辑分区的下标


/**
 * @brief 
 * 选择要操作的硬盘
 */
static void select_disk(disk* hd){
    uint8_t reg_device = BIT_DEV_MBS|BIT_DEV_MOD_LBA; //初始化固定值
    if(hd->dev_no==1){ //从盘
        reg_device|=BIT_DEV_DEV;
    }
    outb(reg_device(hd->my_channel),reg_device);
}
/**
 * @brief 
 * 向硬盘控制器写入起始扇区和要读写的扇区大小
 */
static void select_sector(disk* hd,uint32_t lba,uint8_t sec_cnt){//

    ide_channel* channel = hd->my_channel;

    //写入要读取的扇区数
    
    outb(reg_sect_cnt(channel),sec_cnt); //sec_cnt ==0,读写256个扇区,扇区物理寻址从1开始最大256，LBA寻址从0开始最大255

    //起始扇区号,写入lba地址低24位
    outb(reg_lba_l(channel),lba);
    outb(reg_lba_m(channel),lba>>8);
    outb(reg_lba_h(channel),lba>>16);

    //写入lba高4位
    uint8_t reg_device = BIT_DEV_MBS|BIT_DEV_MOD_LBA|(hd->dev_no?BIT_DEV_DEV:0)|(lba>>24);//三目运算符写反,排错4个小时
    outb(reg_device(channel),reg_device);

}
/**
 * @brief 
 * 向通道发送命令
 */
static void cmd_out(ide_channel* channel,uint8_t cmd){
    //期望返回消息
    channel->expecting_intr = true;
    outb(reg_cmd(channel),cmd);

}
/**
 * @brief 
 * 从硬盘读取sec_cnt个扇区到buf
 */
static void read_from_sector(disk* hd ,void* buf,uint8_t sec_cnt){
    uint32_t size_byte = 0;
    if(sec_cnt)size_byte= sec_cnt*512;
    else size_byte= 256*512;
    insw(reg_data(hd->my_channel),buf,size_byte/2);
}
/**
 * @brief 
 * 将buf中sec_cnt个扇区数据写入到硬盘
 */
static void write2sector(disk* hd,void* buf,uint8_t sec_cnt){
    uint32_t size_byte = 0;
    if(sec_cnt)size_byte= sec_cnt*512;
    else size_byte= 256*512;

    outsw(reg_data(hd->my_channel),buf,size_byte/2);
}
/**
 * @brief 
 * 检查硬盘读出数据是否准备就绪
 * 如果硬盘数据输出准备就绪返回TRUE,超时返回FALSE
 */
static bool busy_wait(disk* hd){
    ide_channel* channel = hd->my_channel;
    uint16_t sleep_time = 30*1000;
    while (sleep_time)
    {   
        if(!(inb(reg_status(channel))&BIT_ALT_STAT_BSY)){
            return inb(reg_status(channel))&BIT_ALT_STAT_DRQ; //准备好数据随时可以输出
        }else{
            sleep(10);
        }
        sleep_time-=10;
    }
    return false;
}

/**
 * @brief 
 * 从硬盘(hd)读取(sec_cnt)个扇区到缓冲区(buf)
 * 没有发生错误返回true,读取错误返回false
 */
bool ide_read(disk* hd,uint32_t lba,void* buf, uint32_t sec_cnt){
    if(lba>max_lba){
        dbg_printf("error ide.c ide_read\n");
        return false;
    }
    lock_acquire(&hd->my_channel->lock);

    ide_channel* channel = hd->my_channel;

    //选择要设置的硬盘
    select_disk(hd);

    uint32_t secs_op; //每次操作扇区数
    uint32_t secs_done = 0;//已完成的扇区数
    while (secs_done<sec_cnt)
    {
        if(sec_cnt-secs_done>=256){
            secs_op=256;   //一次读取最多256个扇区,物理扇区数从1开始计算
        }else{
            secs_op = sec_cnt-secs_done;
        }
        //写入待读取的起始扇区号和扇区数
        select_sector(hd,lba+secs_done,secs_op);

        //执行的读取命令写入reg_cmd寄存器
        cmd_out(channel,CMD_READ_SECTOR);

        //阻塞自己，等待硬盘中断处理程序唤醒
        sema_down(&hd->my_channel->disk_done);

        //硬盘返回中断后,检查硬盘是否准备好数据
        if(!busy_wait(hd)){
            dbg_printf("error ide_read,%s read sector %d failed ! ! !\n",hd->name,lba+secs_done);
            return false;
        }

        //把数据从硬盘缓冲区中取走
        read_from_sector(hd,(void*)((uint32_t)buf+secs_done*512),secs_op);//写入到buf+已完成扇区数字节的偏移
        secs_done+=secs_op;
    }
    lock_release(&hd->my_channel->lock);
    return true;
}

/**
 * @brief 
 * 将buf中的sec_cnt个扇区大小(512 byte)的数据写入到磁盘
 */
void ide_write(disk* hd,uint32_t lba,void* buf, uint32_t sec_cnt){
     if(lba>max_lba){
        dbg_printf("error ide.c ide_write\n");
        return;
    }
    //dbg_printf("ide write lba:0x%x,buf:0x%x\n",lba,(uint32_t)buf);
    lock_acquire(&hd->my_channel->lock);

    ide_channel* channel = hd->my_channel;

    //选择要设置的硬盘
    select_disk(hd);

    uint32_t secs_op; //每次操作扇区数
    uint32_t secs_done = 0;//已完成的扇区数
    while (secs_done<sec_cnt)
    {
        if(sec_cnt-secs_done>=256){
            secs_op = 256;   //一次读取最多256个扇区,物理扇区数从1开始计算
        }else{
            secs_op = sec_cnt-secs_done;
        }
        //写入待写入的起始扇区号和扇区数
        select_sector(hd,lba+secs_done,secs_op);

        //执行的写入命令写入reg_cmd寄存器
        cmd_out(channel,CMD_WRITE_SECTOR);



        if(!busy_wait(hd)){
            dbg_printf("error ide_write,%s write sector %d failed ! ! !\n",hd->name,lba+secs_done);
            return;
        }
        
        //将数据写入到硬盘
        write2sector(hd,(void*)((uint32_t)buf+secs_done*512),secs_op);//bug,secs_done写成了secs_op
        
        //阻塞自己，等待硬盘中断处理程序唤醒
        sema_down(&hd->my_channel->disk_done);

        secs_done+=secs_op;
    }
    lock_release(&hd->my_channel->lock);
    return;
}
/**
 * @brief 
 * 硬盘中断处理程序
 */
void ide_hd_handler(uint8_t irq_no){
    assert(irq_no==0x2e||irq_no==0x2f);
    //取指针类型，方便使用命令块宏
    ide_channel* channel = &channels[irq_no-0x2e];
    assert(channel->irq_no==irq_no)
    if(channel->expecting_intr){
        channel->expecting_intr=false;
        sema_up(&channel->disk_done);
        //通过读取状态寄存器，让硬盘继续处理其他中断请求
        inb(reg_status(channel));
    }
}
/**
 * @brief 
 * 将dst中len个相邻字节交换位置后存入buf
 */
static void swap_pairs_bytes(const char* dst,char* buf,uint32_t len){
    uint8_t idx;
    for (idx = 0; idx < len; idx+=2)
    {
        buf[idx+1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = '\0';
    
}
/**
 * @brief 
 * 获取硬盘信息
 */
static void identtify_disk(disk* hd){
    char id_info[512];

    select_disk(hd);
    cmd_out(hd->my_channel,CMD_IDENTIFY);

    sema_down(&hd->my_channel->disk_done);//阻塞，等待硬盘中断返回

    if(!busy_wait(hd)){//硬盘操作失败
        dbg_printf("error,ide.c->identtify_disk->busy_wait\n");
        return;
    }
    read_from_sector(hd,id_info,1);//读取硬盘信息到缓冲区

    char buf[64];
    uint8_t sn_start = 10*2,sn_len= 20,md_start= 27*2,md_len =40;
    swap_pairs_bytes(&id_info[sn_start],buf,sn_len);
    dbg_printf("disk %s\n  info:SN:%s\n",hd->name,buf);
    
    memset(buf,0,sizeof(buf));

    swap_pairs_bytes(&id_info[md_start],buf,md_len);
    dbg_printf("  MODULE:%s\n",buf);
    uint32_t sectors = *(uint32_t*)&id_info[60*2];
    dbg_printf("  SECTORS: %d\n",sectors);
    dbg_printf("  CAPACITY:%dMB\n",sectors*512/1024/1024);
}
/**
 * @brief 
 * 扫描硬盘hd中起始地址为ext_lba中的所有分区
 */
static void partition_scan(disk* hd,uint32_t ext_lba){

    boot_sector* bs = sys_malloc(sizeof(boot_sector));
    ide_read(hd,ext_lba,bs,1);
    uint8_t part_idx = 0; 
    partition_table_entry* p = bs->partition_table;
    
    while (part_idx++<4)//一个硬盘，最多4个分区表
    {   
        
        if(part_idx==1){
            p+=4; //指向mbr末尾
            if(*(uint16_t*)(p)!=0xaa55){
                dbg_printf("margic error\n");
            }else{
                dbg_printf("margic success\n");
            }
            p-=4;
        }
        if(p->fs_type ==0x5)
        {//扩展分区,在给硬盘分区时,自定义的类型
            if(ext_lba_base!=0){
                partition_scan(hd,p->start_lba+ext_lba_base);
            }else{//表示是第一次读取引导块
                ext_lba_base = p->start_lba;
                partition_scan(hd,p->start_lba);
            }
        }else if(p->fs_type!=0)
        {//有效的分区类型
            if(ext_lba == 0){ //主分区
                hd->prim_parts[p_no].start_lba = p->start_lba;
                hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;
                list_append(&partition_list,&hd->prim_parts[p_no].part_tag);
                //清空分区name缓冲区
                memset(hd->prim_parts[p_no].name,0,8);
                sprintf(hd->prim_parts[p_no].name,"%s%d",hd->name,p_no+1);//主分区编号从1到4
                p_no++;
                assert(p_no<4);
            }else{//逻辑分区
                hd->logic_parts[l_no].start_lba = ext_lba+p->start_lba;
                hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
                hd->logic_parts[l_no].my_disk = hd;
                list_append(&partition_list,&hd->logic_parts[l_no].part_tag);
                memset(hd->logic_parts[l_no].name,0,8);
                sprintf(hd->logic_parts[l_no].name,"%s%d",hd->name,l_no+5);//逻辑分区的起始编号从5开始
                l_no++;
                if(l_no>=8){ //结构体目前只支持8个逻辑分区
                    return;
                }
            }
        }
        p++;
    }
    sys_free(bs); //回收内存
}
/**
 * @brief 
 * 打印分区信息
 */
static bool partition_info(list_elem* pelem,int arg){

    partition* part = (partition*)struct_entry(pelem,partition,part_tag);
    dbg_printf("%s start_lba:0x%x,sec_cnt:0x%x\n",part->name,part->start_lba,part->sec_cnt);
    return false;
}

/**
 * @brief 
 * 硬盘数据结构初始化
 */
void ide_init(void){
    ext_lba_base =0;
    p_no = 0,l_no = 0; 

    uint8_t hd_cnt = *(uint8_t*)(0x475); //硬盘数量
    uint8_t dev_no = 0;
    channel_cnt = (hd_cnt+2-1)/2; //一个通道可管理两块硬盘
    ide_channel* channel;
    uint8_t channel_no = 0;
    list_init(&partition_list);//初始化分区链表
    while (channel_no<channel_cnt)
    {
        channel = &channels[channel_no];
        sprintf(channel->name,"ide%d",channel_no); //通道名赋值
        
        //配置ide0和ide1的通道结构体信息,实际上我们只使用了ide0
        switch(channel_no)
        {
            case 0://ide0
            {   
                channel->port_base = 0x1f0;
                channel->irq_no = 0x20+14; //interrupt中设置的8259a起始中断向量号+irq偏移
                break;
            }
            case 1://ide1
            {
                channel->port_base = 0x170;
                channel->irq_no = 0x20+15;
                break;
            }
        }
        channel->expecting_intr = false;//不期望获得硬盘中断
        lock_init(&channel->lock);
        seam_init(&channel->disk_done,0);//初始化为0,,第一次调用down的时，线程就阻塞，因为cmd_out设置了期望获得硬盘中断返回，所以由硬盘中断处理程序唤醒线程。
        
        register_handler(channel->irq_no,ide_hd_handler); //注册中断处理程序
        while (dev_no <2)
        {
            disk* hd = &channel->devices[dev_no];
            memset(hd,0,sizeof(disk)); //保证所有成员初始化0，方便后面是否存在分区判断以及分区名称copy
            hd->my_channel = channel;
            hd->dev_no = dev_no;//主盘为0，从盘为1
            sprintf(hd->name,"sd%c",'a'+channel_no*2+dev_no);
            identtify_disk(hd);
            if(dev_no==1){
                partition_scan(hd,0);
            }
            p_no = 0,l_no = 0;
            dev_no++;
        }
        dev_no=0;
        channel_no++;
    }
    dbg_printf("\nall partition info:\n");
    list_traversal(&partition_list,(functionc*)partition_info,NULL);
    dbg_printf("ide_init success\n");
}