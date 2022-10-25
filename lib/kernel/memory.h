#ifndef __LIB_MEMORY_H
#define __LIB_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"
#define MEM_BITMAP_BASE 0xc009a000
#define K_HEAD_START 0XC0100000
#define total_mem_bytes 0x0b74 //load.bin 保存物理内存容量的地址
/*PDE和PTE属性(不需要移位)*/
#define PG_P_1 1    //存在物理内存中lib/kernel/memory.h
#define PG_P_0 0    //不在物理内存中
#define PG_RW_R 0   //可读不可写
#define PG_RW_W 2   //可读可写
#define PG_US_S 0   //系统级,三环不允许访问
#define PG_US_U 4   //用户级,所以特权级均可访问
#define PDE_IDX(addr) ((addr&0xffc00000)>>22)   //获取pde索引
#define PTE_IDX(addr) ((addr&0x003FF000)>>12)   //获取pte索引
#define DESC_CNT 7 //7种内存块描述符
enum pool_flags{    //虚拟内存池标志,用于判断访问哪个虚拟内存池
    PF_KERNEL = 1,
    PF_USER
};
//虚拟内存池
typedef struct _virtual_pool
{
    BitMap bitmap_vaddr; //虚拟地址用到的位图结构
    uint32_t vaddr_start; //申请虚拟地址的起始地址
}virtual_pool;
/*内核和用户的物理内存池结构体*/
extern struct _pool kernel_pool,user_pool;
/*arenar */
//内存块
typedef struct _mem_block
{
    list_elem free_elem;
}mem_block;
//内存块描述符
typedef struct _mem_block_desc
{
    uint32_t block_size;//此内存块所描述的内存块大小
    uint32_t blocks_per_arena;//一个arena所能包含block的最大数量
    list free_list;//可用的内存块(mem_block)链表
}mem_block_desc;

/*                  方法区                  */
/*
功能:在指定的内存池中,申请指定起始地址的一页内存。
参数:vaddr,指定的起始虚拟地址。pool_flags,指定的内存池.
返回:成功返回起始地址,失败返回NULL
*/
void* get_a_page(enum pool_flags pf,uint32_t vaddr);
/*
功能:将虚拟地址转为物理地址
返回:物理地址
*/
uint32_t addr_v2p(uint32_t vaddr);
/*
memory模块初始化
*/
void mem_init(void);
/*
功能:在内核申请pg_cnt个内核页,返回起始虚拟地址
返回:内核页的起始地址,失败返回NULL
*/
void* get_kernel_pages(uint32_t pg_cnt);
/*
功能:在用户进程虚拟内存池中分配pg_cnt个内存页
返回:内存页的起始地址,失败返回NULL
*/
void* get_user_pages(uint32_t pg_cnt);
/*
功能:初始化内存块描述符数组中的所有内存块描述符
参数:desc_array,内存块描述符数组
*/
void block_desk_init(mem_block_desc* desc_array);
/*
在堆中申请内存
成功返回起始地址，失败返回NULL
*/
void* sys_malloc(uint32_t size);
/*
回收物理地址pg_phy_addr所在的内存页
*/
void pfree(uint32_t pg_phy_addr);
/**
 * @brief 
 * 回收虚拟地址,以及_vaddr处开始的连续cnt个物理页
 */
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t cnt);
/**
 * @brief 
 * 回收指针ptr指向的内存
 * 只有通过sys_malloc申请的内存才能通过此函数释放
 */
void sys_free(void* ptr);
/*
打印输出内存池位图情况
*/
void debug_mem_pool(void);
/**
 * @brief 
 * 安装1页大小的vaddr,供fork使用无需设置虚拟地址位图
 * 成功返回虚拟地址,失败返回NULL
 */
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf,uint32_t vaddr);
/*
功能:获取虚拟地址vaddr的pde指针
返回:pde指针(虚拟地址)
*/
 uint32_t * get_pde_ptr(uint32_t vaddr);
 /*
功能:获取虚拟地址vaddr的pte指针
返回:pte指针(虚拟地址)
*/
 uint32_t * get_pte_ptr(uint32_t vaddr);
/**
 * @brief 
 * 回收指定物理内存地址起始的一个页框大小(只回收了内存池位图，未对页表进行更改)
 */
void free_a_phy_page(uint32_t pg_phy_addr);
#endif