#include "exec.h"
#include "../lib/kernel/memory.h"
#include "../fs/file.h"
#include "../fs/fs.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../lib/stdio.h"
extern void intr_exit(void);
typedef uint32_t Elf32_Word,Elf32_Addr,Elf32_Off;
typedef uint16_t Elf32_Half;

/*  32位ELF头*/
typedef struct
{
    unsigned char e_ident[16];     /* Magic number and other info */
    Elf32_Half    e_type;                 /* Object file type */
    Elf32_Half    e_machine;              /* Architecture */
    Elf32_Word    e_version;              /* Object file version */
    Elf32_Addr    e_entry;                /* Entry point virtual address */
    Elf32_Off     e_phoff;                /* Program header table file offset */
    Elf32_Off     e_shoff;                /* Section header table file offset */
    Elf32_Word    e_flags;                /* Processor-specific flags */
    Elf32_Half    e_ehsize;               /* ELF header size in bytes */
    Elf32_Half    e_phentsize;            /* Program header table entry size */
    Elf32_Half    e_phnum;                /* Program header table entry count */
    Elf32_Half    e_shentsize;            /* Section header table entry size */
    Elf32_Half    e_shnum;                /* Section header table entry count */
    Elf32_Half    e_shstrndx;             /* Section header string table index */
} Elf32_Ehdr;

/*程序头表*/
typedef struct
{
    Elf32_Word  p_type;          /* Segment type */
    Elf32_Off   p_offset;        /* Segment file offset */
    Elf32_Addr  p_vaddr;         /* Segment virtual address */
    Elf32_Addr  p_paddr;         /* Segment physical address */
    Elf32_Word  p_filesz;        /* Segment size in file */
    Elf32_Word  p_memsz;         /* Segment size in memory */
    Elf32_Word  p_flags;         /* Segment flags */
    Elf32_Word  p_align;         /* Segment alignment */
}Elf32_Phdr;

/* 段类型*/
enum segment_type{
    PT_NULL,    //无用
    PT_LOAD,    //可加载程序段
    PT_DYNAMIC, //动态加载信息
    PT_INTERP,  //动态加载器名称
    PT_NOTE,    //一些辅助信息
    PT_SHLTB,   //保留
    PT_PHDR     //程序头表
};

//存放地址对应关系
uint32_t* target_addr_array;
uint32_t* buffer_addr_array;
uint32_t* size_arrary;
int32_t addr_idx;



/**
 * @brief 
 * 将文件描述符fd指向的文件中,偏移为offset大小为filesz的段加载到指定虚拟地址(vaddr) 
 * 成功返回true,失败返回false
 */
static bool segment_load(
uint32_t fd,uint32_t offset,uint32_t filesz,uint32_t vaddr){
    int32_t pg_cnt = DIV_ROUND_UP(filesz,PG_SIZE);
    void* temp_buf = get_kernel_pages(pg_cnt);
    assert(temp_buf!=NULL);
    //dbg_printf("load addr 0x%x,temp_buf:0x%x,filesz:%d,pg_cnt:%d\n",vaddr,temp_buf,filesz,pg_cnt);              
    //虚拟地址所在页的起始地址(页框)
    uint32_t vaddr_fist_page = vaddr&0xfffff000;
    //段加载到内存中后，第一个页框可提供的内存大小
    uint32_t size_in_fist_page = PG_SIZE - (vaddr & 0x00000fff);

    // 1.判断(以vaddr开始的)一个页框是否能容纳整个段
    uint32_t occupy_pages = 0; //被加载段需要的页框数量
    if(filesz > size_in_fist_page){
        //容纳不下,计算需要的页框数量
        uint32_t left_size = filesz - size_in_fist_page;
        occupy_pages = DIV_ROUND_UP(left_size,PG_SIZE);                     
    }
    occupy_pages++;
    //dbg_printf("occupy_pages %d\n",occupy_pages);
    // 2.为写入段准备好可用的vaddr。
    uint32_t page_idx = 0;
    uint32_t vaddr_page = vaddr_fist_page;
    while(page_idx < occupy_pages){
        uint32_t* pde = get_pde_ptr(vaddr_page);
        uint32_t* pte = get_pte_ptr(vaddr_page);
        //判断原先的进程申请过此内存地址
        //判断pde和pte 的有效位
        if(!(*pde & 1) || !(*pte & 1)){
            //未申请过此内存
            if(get_a_page(PF_USER,vaddr_page) == NULL){
                //申请失败,回收已申请成功的内存
                printf("segment_load get_page error\n");
                mfree_page(PF_USER,(void*)vaddr_fist_page,page_idx);
                return false;
            }
        }
        //已申请过此内存,无需再次申请
        vaddr_page +=PG_SIZE;
        page_idx++; 
    }
    // 3.将文件内容写入到vaddr
    sys_lseek(fd,offset,SEEK_SET);

    /**
     * bug 点
     * 问题:
     *      1.
     *          因为vaddr是有磁盘中的程序指定的要加载的目标地址,当此目标地址与内存中的arena的地址重合时,
     *      将会导致在执行sys_read时，使用磁盘中的程序数据覆盖当前arena的内存信息,导致sys_read
     *      中的sys_free执行失败。
     *      2.
     *          也可以这样子想,因为sys_read中存在sys_malloc函数,此函数根据当前进程所在的地址空间，来划分arena，
     *          当进程处于用户模式时，arena也处于用户地址空间。如果直接使用书中的memcpy函数，则可能导致arena被破坏
     *          导致后面的sys_free失败.
     *      
     * 解决办法:
     *      使用额外的内核缓冲区接收程序中的数据,memcpy覆盖内存中的数据的操作应该留到最后，也就是在所有涉及到用户地址空间操作之后。
    */

    int32_t read_sz = sys_read(fd,temp_buf,filesz);
    if(read_sz != (int32_t)filesz){
        printf("segment_load: read file size  fault\n");
        return false;
    }
    target_addr_array[addr_idx] = vaddr;
    buffer_addr_array[addr_idx] = (uint32_t)temp_buf;
    size_arrary[addr_idx] = filesz;
    addr_idx++;
    //dbg_printf("segment_load end\n");
    return true;
}

/**
 * @brief 
 * 从文件系统上加载用户程序 pathname
 * 成功返回程序的起始地址
 * 失败返回-1
 */
static int32_t load(const char* pathname){
    int32_t ret = -1;
    Elf32_Ehdr elf_header;
    Elf32_Phdr prog_header;
    
    memset(&elf_header,0,sizeof(Elf32_Ehdr));

    // 1.打开文件
    int32_t fd = sys_open(pathname,O_READ);
    if(fd ==-1){
        return -1;
    }
    /*读取elf头*/
    if(sys_read(fd,&elf_header,sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)){
        goto done;
    }
    // 2.比较elf文件头
    if(memcmp(elf_header.e_ident,"\177ELF\1\1\1",7) \
    || elf_header.e_type != 2 \
    || elf_header.e_machine !=3 \
    || elf_header.e_version !=1 \
    || elf_header.e_phnum > 1024 \
    || elf_header.e_phentsize != sizeof(Elf32_Phdr)){
        ret = -1;
        printf("elf header faild\n");
        goto done;
    }

    //存放地址对应关系 注意使用内核空间
    target_addr_array = get_kernel_pages(1);
    buffer_addr_array = get_kernel_pages(1);
    size_arrary = get_kernel_pages(1);
    addr_idx= 0;
    assert(target_addr_array!=NULL && buffer_addr_array!=NULL && size_arrary!=NULL);

    // 3.通过elf程序头,读取段内容到内存中
    Elf32_Off prog_header_offset = elf_header.e_phoff;
    Elf32_Half prog_header_size = elf_header.e_phentsize;

    uint32_t prog_idx = 0;
    while(prog_idx < elf_header.e_phnum)
    {
        memset(&prog_header,0,prog_header_size);

        /*将文件的指针定位到程序头*/
        sys_lseek(fd,prog_header_offset,SEEK_SET);

        /*读取程序头*/
        //dbg_printf("sys_read kaishi\n");
        if(sys_read(fd,&prog_header,prog_header_size) != prog_header_size)
        {
            ret = -1;
            //dbg_printf("prog_header read faild\n");
            goto done;
        }
            //dbg_printf("sys_read end\n");
        /*如果是可加载段就调用 segment_load 载入堆中*/
        if(PT_LOAD == prog_header.p_type)
        {
            if(!segment_load(fd,prog_header.p_offset,prog_header.p_filesz,prog_header.p_vaddr))
            {
                ret = -1;
                //dbg_printf("segment_load faild\n");
                goto done;
            }
        }
        /*下一个程序头*/
        prog_header_offset+=prog_header_size;
        prog_idx++;
       // printf("prog_idx:%d,prog_header_offset:0x%x \n",prog_idx,prog_header_offset);
    }

    ret = elf_header.e_entry;
    
    //操作失败,还原步骤
done:
    sys_close(fd);
    return ret;
    //dbg_printf("load end\n");
}
/**
 * @brief 
 * 用path指向的程序替换当前进程
 * 成功不返回，失败返回 -1
 */
int32_t sys_exec(const char* path, const char* argv[]){
    // 1.初始化
    uint32_t argc = 0;
    while(argv!=NULL&& argv[argc]){
        argc++;
    }
    int32_t entry_point = load(path);
    if(entry_point == -1){
        return -1;
    }
    /*因为内核空间不会被覆盖,后面也没有用户地址空间的使用，所以决定在这里覆盖*/
    uint16_t m_idx = 0;
    while(addr_idx){
        //dbg_printf("addr_idx:%d\n",addr_idx);
        memcpy((void*)target_addr_array[m_idx],(void*)buffer_addr_array[m_idx],size_arrary[m_idx]);
        mfree_page(PF_KERNEL,(void*)buffer_addr_array[m_idx],1);
        //dbg_printf("target addr:0x%x,buffer addr:0x%x,size:0x%x\n",target_addr_array[m_idx],buffer_addr_array[m_idx],size_arrary[m_idx]);
        addr_idx--;
        m_idx++;
    }
    mfree_page(PF_KERNEL,target_addr_array,1);
    mfree_page(PF_KERNEL,buffer_addr_array,1);
    mfree_page(PF_KERNEL,size_arrary,1);
    //2.替换进程
    task_struct* cur = get_running_thread_pcb();

    memcpy(cur->name,(char*)path,TASK_NAME_LEN); //这里想改一下，后面测试完回来改变
    cur->name[TASK_NAME_LEN-1] = 0;
    intr_stack* intr_0_stack = (intr_stack*)((uint32_t)cur + PG_SIZE -sizeof(intr_stack));
    /*传递参数给子进程*/
    intr_0_stack->ebx = (int32_t)argv; 
    intr_0_stack->ecx = argc;
    intr_0_stack->old_eip = (void*) entry_point;
    /*修改新进程的栈地址*/
    intr_0_stack->esp = 0xc0000000;
    //dbg_printf("***new task start performing***\n");
    asm volatile ("movl %0,%%esp; jmp intr_exit" : :"g" (intr_0_stack) : "memory");
    return 0; //不会被执行
}

