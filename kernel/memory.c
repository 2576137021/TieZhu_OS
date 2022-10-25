#include "../lib/kernel/memory.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../thread/thread.h"
#include "../thread/sync.h"
//整备安排内存位图的地址
/*
因为0x9f000是内核栈顶,而以后还会给内核进程创建一个pcb,pcb位置预定为0x9e000(pcb必须在一页内)。
我们系统的内存大小不同于书上,是512MB,
计算一下需要多少内存来存放内存位图.
一个页表的所占内存 =4bit*1024 = 4096bit
一个页表的内存用于初始化位图可以表示多少内存  =  4096*8(多少位)*4(“4k”.位图的一位表示4k的内存块) = 131072KB = 128MB
所以512MB的内存需要 512/128 = 4页的地址空间来标识
bit_map_base = 0x9e000-(4*0x1000) = 0x9a000 = 0xc09a000
为什么内存位图要放置在虚拟地址的高位即低端物理地址1MB之下呢？
    因为低端1mb为系统内核地址,在之后计算剩余物理地址的时候,只需要减去这1MB而不需要考虑位于其他物理地址的bit_map_base的大小
*/
/*
物理内存池
*/
typedef struct _pool
{
    BitMap pool_bitmap; //物理内存池的位图结构
    uint32_t phy_addr_start;//物理内存池的起始地址
    uint32_t pool_size;  //物理内存池的容量,最大支持4095MB
    lock lock;//申请内存时互斥
}pool,*Ppool;
pool kernel_pool,user_pool;//内核物理内存池和用户物理内存池
virtual_pool kernel_vaddr,user_vaddr;//内核虚拟地址池,用户虚拟地址池

/*内存仓库 
整个arena的大小为0x1000字节,除去arena的元信息，剩下的都为内存块
*/
typedef struct _arena
{
    mem_block_desc * desc;//此arena关联的内存块描述符
    uint32_t cnt; //large = true,页框数.false = 空闲的内存块数量.
    bool large;
}arena;
mem_block_desc k_block_descs[DESC_CNT]; //内核内存块描述符数组


/*
功能:初始化物理内存池
参数:物理内存总容量 
ps 函数中所有的内存单位都是字节
*/
static void mem_pool_init(uint32_t all_mem){

    /*
    计算一下除了低端1MB以外已经被使用的物理内存大小
    参考注释 loader.s line 181 - 222
    254个页目录项+第一个页目录项(pdt)+最后一个页目录项(指向pdt) = 256个页目录项(pde)
    1个pde = 1024*4B = 4096 B
    已经被占用的物理内存 = 4096B *256 = 0x100000 B = 1024KB = 1MB
    */
   uint32_t used_mem = (PG_SIZE*256)+0x100000;             //已占用的物理内存 = 分页所占内存+低端1MB的内存  debug addr: 0xc000226b
   uint32_t free_mem = all_mem - used_mem;      //空闲的物理内存
   uint16_t all_free_pages = free_mem/PG_SIZE;  //空闲的物理内存页数量,不足一页的内存将被丢弃
   
   uint16_t kernel_free_pages = all_free_pages/2;//内核占物理内存的一半,这是我们设计的方案
   uint16_t user_free_pages = all_free_pages-kernel_free_pages;
   
   uint32_t kbm_length = kernel_free_pages/8; //描述内核物理内存的位图的字节大小 kbm = kernel bit map
   uint32_t ubm_length = user_free_pages/8;   //同上,用户物理内存位图的字节大小

   uint32_t kp_start = used_mem;                            //内核可用物理内存起始地址 kp = kernel point
   uint32_t up_start = kp_start+kernel_free_pages*PG_SIZE; //用户可用物理内存起始地址

   //初始化内核和用户物理内存池结构体
   kernel_pool.phy_addr_start = kp_start;
   kernel_pool.pool_size = kernel_free_pages*PG_SIZE;//忘记 "*PG_SIZE",7/25回来修改bug
   kernel_pool.pool_bitmap.bitmap_byte_len=kbm_length;
   kernel_pool.pool_bitmap.bits = (uint8_t*)MEM_BITMAP_BASE;

   user_pool.phy_addr_start = up_start;
   user_pool.pool_size = user_free_pages*PG_SIZE;
   user_pool.pool_bitmap.bitmap_byte_len = ubm_length;
   user_pool.pool_bitmap.bits = (uint8_t*)(MEM_BITMAP_BASE+kbm_length);
   
   //打印输出内存池信息
//    print("kernel_pool_start:");
//    put_int32(kernel_pool.phy_addr_start);
//    print("\n");
//    print("kernel_pool.pool_size:");
//    put_int32(kernel_pool.pool_size);
//    print("\nkernel_pool.pool_bitmap.bits:");
//    put_int32((uint32_t)kernel_pool.pool_bitmap.bits);
//    print("\nkernel_pool.pool_bitmap.bitmap_byte_len:");
//    put_int32(kernel_pool.pool_bitmap.bitmap_byte_len);

//    print("\nuser_pool_start:");
//    put_int32(user_pool.phy_addr_start);
//    print("\nuser_pool.pool_size:");
//    put_int32(user_pool.pool_size);
//    print("\nuser_pool.pool_bitmap.bits:");
//    put_int32((uint32_t)user_pool.pool_bitmap.bits);
//    print("\nuser_pool.pool_bitmap.bitmap_byte_len:");
//    put_int32(user_pool.pool_bitmap.bitmap_byte_len);
//    print("\n");

   //初始化位图
   bitmap_init(&kernel_pool.pool_bitmap); //error
   bitmap_init(&user_pool.pool_bitmap);

   //初始化内核虚拟地址位图
   kernel_vaddr.bitmap_vaddr.bitmap_byte_len = kbm_length;
   kernel_vaddr.bitmap_vaddr.bits = (uint8_t*)(MEM_BITMAP_BASE+kbm_length+ubm_length);//暂时定位在pcb的预留空间中
   memset(kernel_vaddr.bitmap_vaddr.bits,0,kbm_length);

   kernel_vaddr.vaddr_start = K_HEAD_START;
   bitmap_init(&kernel_vaddr.bitmap_vaddr);

   lock_init(&kernel_pool.lock);
   lock_init(&user_pool.lock);
}
/*
功能:在pf指定的虚拟内存池中分配pg_cnt个虚拟页(申请虚拟内存)
返回值:成功返回分配的虚拟起始地址,失败返回NULL
*/
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt){
    uint32_t vaddr_start = (uint32_t)NULL;
    int32_t bitMap_start_idx = -1;
    if(pf==PF_KERNEL){
        bitMap_start_idx = bitMap_scan(&kernel_vaddr.bitmap_vaddr,pg_cnt);
        if(bitMap_start_idx==-1)return NULL; //分配内存失败,内存空间不足
        else{
            bitmap_setEx(&kernel_vaddr.bitmap_vaddr,bitMap_start_idx,1,pg_cnt);
            vaddr_start = bitMap_start_idx*PG_SIZE+kernel_vaddr.vaddr_start;
            return (void*)vaddr_start;
        }
    }else{
        //用户虚拟内存分配
        task_struct* cur = get_running_thread_pcb();
        bitMap_start_idx= bitMap_scan(&cur->userprog_vaddr.bitmap_vaddr,pg_cnt);
        if(bitMap_start_idx==-1)return NULL;
        else{
            bitmap_setEx(&cur->userprog_vaddr.bitmap_vaddr,bitMap_start_idx,1,pg_cnt);
            vaddr_start = bitMap_start_idx*PG_SIZE+cur->userprog_vaddr.vaddr_start;
            assert(vaddr_start<(0xc0000000-PG_SIZE));//(0xc0000000-PG_SIZE) 是预留给用户进程的3环栈空间
            return (void*)vaddr_start;
        }
    }
}
/*
需求：通过构建一个虚拟地址(new_vaddr),使其指向的物理地址为参数(vaddr)的pte虚拟地址
cpu转换地址三步:
    1.通过前10位得到页表物理地址
    2.通过中间10位得到物理页地址
    3.通过后12位得到物理地址
思路：
    我们要让cpu在最后一步返回的是pte的地址,则第二步需要得到的是页表地址,第一步需要得到的是页目录表地址
问题：
    如何第一步得到页目录表的地址呢？
        我们在划分虚拟内存的时候,将页目录表的最后一项设置为指向页目录表起始地址
    其余两步:
        在第二步的时候,使用参数(vaddr)前10位的偏移,替换new_vaddr的中间10位,即可得到页表地址
        在第三步的时候,使用参数(vaddr)中间10位的偏移*4,替换new_vaddr的后10位,即可得到pte指针
*/

/*
功能:获取虚拟地址vaddr的pte指针
返回:pte指针(虚拟地址)
*/
 uint32_t * get_pte_ptr(uint32_t vaddr){
    uint32_t* pte_ptr = NULL;
    pte_ptr = (uint32_t*)(0xFFC00000+((vaddr&0xFFC00000)>>10)+(PTE_IDX(vaddr)*4));
    return  pte_ptr;
}
/*
功能:获取虚拟地址vaddr的pde指针
返回:pde指针(虚拟地址)
*/
 uint32_t * get_pde_ptr(uint32_t vaddr){
    uint32_t* pde_ptr = NULL;
    pde_ptr = (uint32_t*)(0xFFFFF000+PDE_IDX(vaddr)*4);
    return pde_ptr;
}
/*
功能:在m_pool指定的内存池中分配1个物理页
返回值:成功返回物理页的物理地址,失败返回NULL
*/
static void* palloc(pool* m_pool){
    uint32_t pAddr_start = (uint32_t)NULL;
    int32_t map_idx = bitMap_scan(&m_pool->pool_bitmap,1);
    if(map_idx!=-1){
        bitmap_set(&m_pool->pool_bitmap,map_idx,1);
        pAddr_start = map_idx*PG_SIZE+m_pool->phy_addr_start;
    }
    return (void*)pAddr_start;
}
/*
功能:在页表中添加虚拟地址_vaddr与物理页_page_phyaddr的映射
*/
static void page_table_add(void* _vaddr,void* _page_phyaddr){
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = get_pde_ptr(vaddr);
    uint32_t* pte = get_pte_ptr(vaddr);
    //debug
    // print("\n**********page_table_add*********\n");
    // print("vaddr:");
    // put_int32(vaddr);
    // print("\n");
    // print("page_phyaddr:");
    // put_int32(page_phyaddr);
    // print("\n");
    // print("pdeptr:");
    // put_int32((uint32_t)pde);
    // print("\n");
    // print("pteptr:");
    // put_int32((uint32_t)pte);    
    // print("\n**********page_table_add*********\n");
    //通过pde的p位,判断是否存在pde
    if(*pde&PG_P_1){//存在页目录项
        if(*pte&PG_P_1){return;}//存在pte
        else{
            *pte = page_phyaddr|PG_P_1|PG_RW_W|PG_US_U;
        }
    }else{//不存在页目录项
        uint32_t ptt_phyaddr = (uint32_t)palloc(&kernel_pool); //申请页表空间
        *pde = ptt_phyaddr|PG_P_1|PG_RW_W|PG_US_U; //要先设置*pde的值,否则memset会出现pg错误
        memset((void*)((uint32_t)pte&0xfffff000),0,PG_SIZE); //通过pte清空整个页表
        *pte = page_phyaddr|PG_P_1|PG_RW_W|PG_US_U;
    }
    return;
}
/*
功能：分配pg_cnt个页空间
返回值:成功返回虚拟地址起始地址,失败返回NULL
*/
static void* malloc_page(enum pool_flags pf,uint32_t pg_cnt){
    if(pg_cnt ==0){
        pg_cnt=1;
    }
    void* vaddr_start = vaddr_get(pf,pg_cnt);
    if(vaddr_start==NULL){ //申请虚拟内存失败
        return NULL;
    }
    pool* pMem_pool =pf&PF_KERNEL?&kernel_pool:&user_pool;
    uint32_t _vaddr  = (uint32_t)vaddr_start;
    //因为物理地址可以不连续所以需要以页位单位进行申请和绑定
    while (pg_cnt--)
    {
        void* phyaddr_start = palloc(pMem_pool);
        if(phyaddr_start==NULL){//申请物理内存失败
            //回滚虚拟内存池位图 
            //待补充
            return NULL;
        }
        page_table_add((void*)_vaddr,phyaddr_start);//建立映射
        _vaddr+=PG_SIZE;
    }
    return vaddr_start;
}
/*
功能:在内核申请pg_cnt个内核页,返回起始虚拟地址
返回:内核页的起始地址,失败返回NULL
*/
void* get_kernel_pages(uint32_t pg_cnt){
    if(pg_cnt ==0){
        pg_cnt=1;
    }
    lock_acquire(&kernel_pool.lock);  //自己添加的,书上没有
    void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr!=NULL){
        memset(vaddr,0,pg_cnt*PG_SIZE);
    }
    lock_release(&kernel_pool.lock);
    //dbg_printf("****get_kernel_pages:0x%x\n",vaddr);
    return vaddr;
}
/*
功能:在用户进程虚拟内存池中分配pg_cnt个内存页
返回:内存页的起始地址,失败返回NULL
*/
void* get_user_pages(uint32_t pg_cnt){
    if(pg_cnt ==0){
        pg_cnt=1;
    }
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr!=NULL){
        memset(vaddr,0,pg_cnt*PG_SIZE);
    }
    lock_release(&user_pool.lock);
    return vaddr;
}
/*
功能:在指定的内存池中,申请指定起始地址的一页内存。
参数:vaddr,指定的起始虚拟地址。pool_flags,指定的内存池.
返回:成功返回起始地址,失败返回NULL
*/
void* get_a_page(enum pool_flags pf,uint32_t vaddr){
    pool* mem_pool = pf&PF_KERNEL?&kernel_pool:&user_pool;
    lock_acquire(&mem_pool->lock);
    void* ret =NULL;
    task_struct* cur = get_running_thread_pcb();
    int32_t bit_idx = -1;
    //修改位图
    if((cur->pgdir!=NULL)&&(pf==PF_USER)){//用户进程
        assert(vaddr>=cur->userprog_vaddr.vaddr_start);
        bit_idx = (vaddr-cur->userprog_vaddr.vaddr_start)/PG_SIZE;//得到内存地址所在位图索引
        bitmap_set(&cur->userprog_vaddr.bitmap_vaddr,bit_idx,1);
    }else if(cur->pgdir!=NULL&&(pf==PF_KERNEL)){//内核线程
        assert(vaddr>=kernel_vaddr.vaddr_start);
        bit_idx = (vaddr-kernel_vaddr.vaddr_start)/PG_SIZE;//得到内存地址所在位图索引
        bitmap_set(&kernel_vaddr.bitmap_vaddr,bit_idx,1);
    }else{
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_apage");
        return ret;
    }
    //申请物理内存
    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr==NULL)return ret;
    //绑定物理地址和虚拟地址
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}
/*
功能:将虚拟地址转为物理地址
返回:物理地址
*/
uint32_t addr_v2p(uint32_t vaddr){
    uint32_t* pte = get_pte_ptr(vaddr);
    //获得虚拟地址对应的页表项(pte)指针,*pte去掉低12位属性,就是对应的物理页的起始地址+虚拟地址的低12位物理页偏移,就是对应的物理地址
    return ((*pte)&0xfffff000)+(vaddr&0x00000fff);
}
/*
功能:初始化内存块描述符数组中的所有内存块描述符
参数:desc_array,内存块描述符数组
*/
void block_desk_init(mem_block_desc* desc_array){
    uint16_t desc_idx,block_size = 16;
    for( desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
    {
        desc_array[desc_idx].block_size = block_size;
        desc_array[desc_idx].blocks_per_arena = (PG_SIZE-sizeof(arena))/block_size;
        list_init(&desc_array[desc_idx].free_list);
        block_size*= 2; //初始化 16 32 64 128 256...1024 byte的内存块描述符
    }
     
}
/*
返回arena中第idx个内存块的地址
*/
static mem_block* arena2block(arena* a,uint32_t idx){
    return (mem_block*)((uint32_t)a+sizeof(arena)+idx*a->desc->block_size);
}
/*
返回内存块b所在的anera地址
*/
static arena* block2arena(mem_block* b){
    return  (arena*)((uint32_t)b&0xfffff000);
}
/*
在堆中申请内存
成功返回起始地址，失败返回NULL
*/
void* sys_malloc(uint32_t size){
    if(size ==0){
        size=1;
    }
   // printf("malloc start size:%d\n",size);
    enum pool_flags PF;
    pool* mem_pool;
    uint32_t pool_size;
    mem_block_desc* descs;
    task_struct* cur = get_running_thread_pcb();
    //判断需要使用的内存池
    if(cur->pgdir==NULL){//内核线程
        PF = PF_KERNEL;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    }else{//用户进程
        PF = PF_USER;
        mem_pool = &user_pool;
        descs = cur->u_block_desc; //因为是数组
    }
    if(size>mem_pool->pool_size){
        printf("sysmalloc too big,MAX_SIZE:%x\n",mem_pool->pool_size);
        return NULL;
    }
    arena* a;
    mem_block * b;
    lock_acquire(&mem_pool->lock);
    //如果分配超过1024字节的内存块，则需另外申请一页内存
    if(size>1024){ //大页内存申请
      //  printf("big page malooc\n");
        uint32_t page_cnt = DIV_ROUND_UP(size+sizeof(arena),PG_SIZE);
        a= malloc_page(PF,page_cnt);
        if(a!=NULL){
            memset(a,0,page_cnt*PG_SIZE);
            a->desc = NULL; //大页，无内存块描述符
            a->cnt = page_cnt;
            a->large = true;
            lock_release(&mem_pool->lock);
            return (void*)(a+1); //返回arena的第一个内存块的起始地址
        }else{
            lock_release(&mem_pool->lock);
            return NULL;
        }
        
    }else{//小页内存申请
       // printf("small page malooc\n");
        uint8_t desc_idx = 0;
        //从内存块描述符数组中找到合适大小的内存块描述符下标
        for(desc_idx;desc_idx<DESC_CNT;desc_idx++){
        //    printf("block_size:%d\n",descs[desc_idx].block_size);
            if(size<=descs[desc_idx].block_size)break;
        }
     //   printf("desc_idx:%d\n",desc_idx);
        if(list_isempty(&descs[desc_idx].free_list))
        { //内存块描述符中不存在空闲的内存块，需要另外申请一个arena
            a = malloc_page(PF,1);
            if(a ==NULL){
                lock_release(&mem_pool->lock);
                return NULL;
            }
            //为新增的arena初始化属性
            memset(a,0,PG_SIZE);
            a->desc = &descs[desc_idx];
            a->cnt = descs[desc_idx].blocks_per_arena;
            a->large = false;
            uint32_t block_idx;
            enum intr_status old_status = intr_disable();
            //拆分arena
            for(block_idx = 0;block_idx<descs[desc_idx].blocks_per_arena;block_idx++){
                b = arena2block(a,block_idx);//返回block_idx下标的内存块的地址  bug 参数填写错误

                assert(!list_elem_find(&a->desc->free_list,&b->free_elem));
                list_append(&a->desc->free_list,&b->free_elem);
            }
            intr_set_status(old_status);
        }
        //分配内存块
        list_elem* free_block = list_pop(&descs[desc_idx].free_list);
        b = (mem_block* )struct_entry(free_block,mem_block,free_elem);
        memset(b,0,descs[desc_idx].block_size);

        a = block2arena(b);
        a->cnt--;
      //  printf("a->cnt:%d\n",a->cnt);
        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}
/*
回收物理地址pg_phy_addr所在的内存页
*/
void pfree(uint32_t pg_phy_addr){
    pool* mem_pool;
    uint32_t bit_idx = 0;
    if(pg_phy_addr>=user_pool.phy_addr_start){//用户物理内存池
        mem_pool = &user_pool;
        bit_idx =(pg_phy_addr-user_pool.phy_addr_start)/PG_SIZE;
    }else if(pg_phy_addr>=kernel_pool.phy_addr_start){//内核物理内存池
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr-kernel_pool.phy_addr_start)/PG_SIZE;
    }else{//内核页表和低端1MB不可释放
        printf("memeory.c pfree error\n");
        return;
    }
    bitmap_set(&mem_pool->pool_bitmap,bit_idx,0);//位图置零
}
/*
回收虚拟地址_vaddr起始的连续pg_cnt个虚拟内存页
注意:只能回收本线程内的虚拟地址
*/
static void vaddr_remove(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt){
    uint32_t bit_idx = 0,vaddr = (uint32_t)_vaddr;
    virtual_pool* mem_pool;
    if(pf==PF_KERNEL){
        bit_idx = (vaddr-kernel_vaddr.vaddr_start)/PG_SIZE;
        mem_pool = &kernel_vaddr;
    }else if(pf==PF_USER){
        task_struct* cur = get_running_thread_pcb();
        bit_idx = (vaddr-cur->userprog_vaddr.vaddr_start)/PG_SIZE;
        mem_pool = &cur->userprog_vaddr;
    }
    bitmap_setEx(&mem_pool->bitmap_vaddr,bit_idx,0,pg_cnt);    
}
/**
 * @brief 
 * 取消vaddr所在的pte指针的物理内存映射(pte的P位置0)
 */
static void page_table_pte_remove(uint32_t vaddr){
    uint32_t* pte = get_pte_ptr(vaddr);
    *pte&=PG_P_0;
    asm volatile("invlpg %0"::"m" (vaddr):"memory");//更新tlb
}
/**
 * @brief 
 * 回收虚拟地址,以及_vaddr处开始的连续cnt个物理页
 */
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t _cnt){
   // printf("mfree_page all \n");
    uint32_t phy_addr,vaddr = (uint32_t)_vaddr,cnt=_cnt;
    phy_addr = addr_v2p(vaddr);

    if(phy_addr>=user_pool.phy_addr_start){
        //_vaddr位于用户物理内存池
        while (cnt--)
        {
            phy_addr = addr_v2p(vaddr);
            assert(phy_addr>=user_pool.phy_addr_start);
            pfree(phy_addr);
            page_table_pte_remove(vaddr);
            vaddr+=PG_SIZE;
        }
        vaddr_remove(pf,_vaddr,_cnt);
    }else if(phy_addr>=kernel_pool.phy_addr_start){
        //_vaddr位于内核物理内存池
        while (cnt--)
        {
            phy_addr = addr_v2p(vaddr);
            assert(phy_addr>=kernel_pool.phy_addr_start);
            pfree(phy_addr);
            page_table_pte_remove(vaddr);
            vaddr+=PG_SIZE;
        }
        vaddr_remove(pf,_vaddr,_cnt);
    }else{
        printf("memory.c mfree_page is error\n");
        return ;
    }
}
/**
 * @brief 
 * 回收指针ptr指向的内存
 * 只有通过malloc申请的内存才能通过此函数释放
 */
void sys_free(void* ptr){
    //printf("sys_free ptr:%x\n",ptr);
    assert(ptr!=NULL);
    enum pool_flags pf;
    pool* mem_pool; //物理内存池
    //判断是线程还是进程
    if(get_running_thread_pcb()->pgdir==NULL){
        //内核线程
        mem_pool = &kernel_pool;
        pf= PF_KERNEL;
    }else{
        mem_pool = &user_pool;
        pf= PF_USER;
    }

    lock_acquire(&mem_pool->lock);
    mem_block* b= ptr;
    arena* a = block2arena(b);
    assert(a->large==true||a->large==false);//简单判断一下是不是malloc申请的内存
    if(a->desc==NULL&&a->large==true){
        //释放大页内存(>1024byte)
        mfree_page(pf,a,a->cnt);//连同arena元信息一起释放
    }else{
        //释放小内存
        //先将内存块(mem_block)放入free_list
        list_append(&a->desc->free_list,&b->free_elem);
        //判断是否整个arena都是空闲的内存块,如果是则回收内存
   //     printf("a->cnt:%d,a->desc->blocks_per_arena:%d\n",a->cnt+1,a->desc->blocks_per_arena);
        if (++a->cnt == a->desc->blocks_per_arena)
        {
            mem_block* b;
            uint32_t blokc_idx = 0;
            while (blokc_idx<a->cnt)
            {
                mem_block* b = arena2block(a,blokc_idx);
                blokc_idx++;
                list_remove(&b->free_elem); //从空闲链表中移除arena中所有的内存块
                
            }
           // printf("free list cnt:%d,mfree_page ptr:%x\n",list_each(&a->desc->free_list),(uint32_t)a);
            mfree_page(pf,a,1);//回收整个arena
            
        }

    }
    lock_release(&mem_pool->lock);
   // printf("mem_pool->lock.holder_repeat_nm:%d\n",mem_pool->lock.holder_repeat_nm);
}
/*
打印输出内存池位图情况
*/
void debug_mem_pool(void){
    //查看前32位bitmap的值
    uint32_t k_vaddr,u_vaddre=0,k_pool,u_pool;
    k_vaddr =  *(uint32_t*)kernel_vaddr.bitmap_vaddr.bits;
    k_pool = *(uint32_t*)kernel_pool.pool_bitmap.bits;
    u_pool = *(uint32_t*)user_pool.pool_bitmap.bits;
    if(get_running_thread_pcb()->pgdir!=NULL){
        u_vaddre =  *(uint32_t*)get_running_thread_pcb()->userprog_vaddr.bitmap_vaddr.bits; 
    }
    //printf("k_vaddr:%d\nu_vaddr:%d\nk_pool:%d\nu_pool:%d\n",k_vaddr,u_vaddre,k_pool,u_pool);
    

}
/**
 * @brief 
 * 安装1页大小的vaddr,供fork使用无需设置虚拟地址位图
 * 成功返回虚拟地址,失败返回NULL
 */
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf,uint32_t vaddr){
    pool* mem_pool = pf&PF_KERNEL?&kernel_pool:&user_pool;
    lock_acquire(&mem_pool->lock);

    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL){

        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}
/**
 * @brief 
 * 回收指定物理内存地址起始的一个页框大小(只回收了内存池位图，未对页表进行更改)
 */
void free_a_phy_page(uint32_t pg_phy_addr){
    pool* mem_pool;
    uint32_t bit_idx = 0;
    //用户内存池的物理地址在高位
    if(pg_phy_addr >= user_pool.phy_addr_start){
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr-user_pool.phy_addr_start)/PG_SIZE;
    }else{
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr-kernel_pool.phy_addr_start)/PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap,bit_idx,0);
}
/*
功能:初始化内存模块
*/
void mem_init(void){
   //获取实际内存容量
   uint32_t mem_byte_size = *(uint32_t*)total_mem_bytes;
   print("real memory size:");
   put_int32(mem_byte_size);
   print("B\n");
   //初始化内存池锁
   lock_init(&user_pool.lock);
   lock_init(&kernel_pool.lock);
   //内存池初始化
   mem_pool_init(mem_byte_size);
   //初始化内存块描述符数组
   block_desk_init(k_block_descs);
   print("mem_init success\n");
}

