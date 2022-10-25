;内核加载程序
%include "bot.inc"
SECTION loader vstart=LOADER_BASE_ADDR
jmp loader_start
getMemSizeError:
	;INT 0x10 功能号:0x13 功能描述:打印字符串
mov sp,LOADER_BASE_ADDR
mov bp,mem_size_err
mov cx,21
mov ax,0x1301
mov bx,0x001f
mov dx,0x0400
int 0x10
jmp $
;构建GDT表,初始化基础段描述符
GDT_BASE:
	;GDT表的第1个段描述符留空,避免未初始化的段选择使用
	dd 0x10000000    
	dd 0x10000000
CODE_DESC:
	dd 0x0000FFFF
	dd DESC_CODE_HIGH4
DATA_STACK_DESC:
	dd 0x0000FFFF
	dd DESC_DATA_HIGH4
VIDEO_DESC:
	dd 0x80000007 ;(BFFFF-B8000+1)/G-1=7 
	dd DESC_VIDEO_HIGH4
GDT_SIZE equ $-GDT_BASE
GDT_LIMIT equ GDT_SIZE-1
times 60 dq 0 ;预留段描述符空位
;定义选择子
SELECTOR_CODE equ 0000000000001_000b+TI_GDT+RPL0
SELECTOR_DATA_STACK equ 0000000000010_000b+TI_GDT+RPL0
SELECTOR_VIDEO equ 0000000000011_000b+TI_GDT+RPL0
;定义GDTR
gdtr_ptr:
	dw GDT_LIMIT
	dd GDT_BASE
loader_msg:
	db 'The protected mode is started successfully'
mem_size_err:
	db 'Get Memory Size Error'
;代码部分
loader_start:
;INT 0x10 功能号:0x13 功能描述:打印字符串
mov sp,LOADER_BASE_ADDR
mov bp,loader_msg
mov cx,42
mov ax,0x1301
mov bx,0x001f
mov dx,0x1800
int 0x10
jmp get_mem_size_start
;---------------------------------------------------------
;			 获取物理内存大小 0x0b78
;---------------------------------------------------------
;保存物理内存大小 单位MB
total_mem_bytes:
	dd 0
;ARDS结构体
ards_struct:
	dd 0		;Low Base
	dd 0
	dd 0		;Low Mem Length
	dd 0
	dd 0		;Type

;中断0x15,功能号0xe820
get_mem_size_start:
	xor ebx,ebx ;第一次使用,要将ebx清零
	mem_loop:
		mov ax,0
		mov es,ax
		mov di,ards_struct
		mov eax,0xe820
		mov ecx,20
		mov edx,0x534d4150
		int 0x15
		jb getMemSizeError
		mov edx,ards_struct
		mov eax,[edx+8]
		add eax,[edx]
		cmp eax,[total_mem_bytes]
		jb no_record
		mov [total_mem_bytes],eax
		no_record:
		test ebx,ebx
	jne mem_loop
;---------------------------------------------------------
;            启动保护模式   debug address :0xbce
;---------------------------------------------------------
protect_mode_start:
;打开A20
in al,0x92
or al,0000_0010b
out 0x92,al
;加载GDT
lgdt [gdtr_ptr]
;CR0的PE置1
mov eax,cr0
or eax,0x00000001
mov cr0,eax
;刷新流水线
jmp dword SELECTOR_CODE:p_mode_start
[bits 32]
p_mode_start:
;初始化各选择子
mov ax,SELECTOR_DATA_STACK
mov ds,ax
mov es,ax
mov ss,ax
mov esp,LOADER_STACK_TOP
mov ax,SELECTOR_VIDEO
mov gs,ax
;---------------------------------------------------------
;            载入内核文件到内存  debug address:0008:0x0000c0f 
;---------------------------------------------------------
mov eax,KERNEL_START_SECTOR
mov ebx,KERNEL_BIN_BASE_ADDR
mov ecx,KERNEL_SECTOR_SIZE
call rd_disk_m_32
;---------------------------------------------------------
;            启动分页模式
;---------------------------------------------------------
;创建和初始化页目录表和页表
call setup_page
;移动显存到高3G内存
mov eax,[gdtr_ptr+2]
or dword [eax+0x18+4],0xC0000000
;移动内核栈到高3G内存
add esp,0xc0000000
;页目录表地址传给cr3
mov eax,PAGE_DIR_TABLE_POS
mov cr3,eax
;修改cr0的pg位 31位
mov eax,cr0
or eax,1_000000000000000_0000000000000000b
mov cr0,eax
;将GDT移动到高3G内存
add dword [gdtr_ptr+2],0xC0000000
;重新加载gdtr
lgdt [gdtr_ptr]
;---------------------------------------------------------
;            初始化内核
;---------------------------------------------------------
call kerne_init
mov esp,LOADER_KERNEL_STACK_TOP
;---------------------------------------------------------
;            打印字符串 debug address:0008:00000cc9
;---------------------------------------------------------
;输出字符 protected mode
mov byte [gs:160],"p"
mov byte [gs:162],"r"
mov byte [gs:164],"o"
mov byte [gs:166],"t"
mov byte [gs:168],"e"
mov byte [gs:170],"c"
mov byte [gs:172],"t"
mov byte [gs:174],"e"
mov byte [gs:176],"d"
mov byte [gs:178]," "
mov byte [gs:180],"m"
mov byte [gs:182],"o"
mov byte [gs:184],"d"
mov byte [gs:186],"e"
jmp KERNEL_ENTRY_VIRTUAL_ADDR





;------------------------------------------------------------------------------------------------------
;            				 				子功能区域
;------------------------------------------------------------------------------------------------------
;---------------------------------------------------------
;            建立页目录表和页表
;---------------------------------------------------------

;页目录项和页表项的安排总思路:
;首先明确我们写的操作系统内核不超过1MB,而现在是段模式,即操作系统内核所在的物理内存空间LOADER_BASE_ADDR-LOADER_BASE_ADDR+1MB = 00:2304-00:105_0880,在第一个页目录项00:00-00：419_4304描述的范围之内 
;因为每个进程的3g(0xC0000000)以上要指向内核空间,每个页目录项描述0x400000地址空间的大小,所以3g对应的页目录项起始序号768(0x300,相对于页目录表的偏移为0xc00),而我们此时将这个页目录项指向=第一个页目录项,这样就将内核4MB的代码映射到了进程的高3g空间。
;清空页目录表所需的内存空间
setup_page:
[bits 32]
	mov eax,PAGE_DIR_TABLE_POS
	mov ecx,4096 ;1024*4
	xor esi,esi
.pdt_clear:
	mov byte [eax+esi],0
	inc esi
	loop .pdt_clear;
;初始化内核地址空间PDE和用户高3g映射内核4MB PDE
.create_pde:
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x1000
	mov ebx,eax	;此时ebx是第一个页表的物理地址,第一个页表也就是指向内核空间的页表
	or eax,PAGE_US_U|PAGE_RW_RW|PAGE_P
	mov [PAGE_DIR_TABLE_POS],eax		;给第一个页目录项赋值,使其指向0x1001000处的页表(第一个页表正好位于页目录表之上0x1000偏移处)
	mov [PAGE_DIR_TABLE_POS+0xc00],eax	;PAGE_DIR_TABLE_POS指向的位置为页目录表,给第768项页表赋值也让其指向第一个页表
	
	mov eax,PAGE_DIR_TABLE_POS
	or eax,PAGE_US_U|PAGE_RW_RW|PAGE_P
	mov [PAGE_DIR_TABLE_POS+0xffc],eax	;页目录表的最后一项指向页目录表的基质
;初始化第一个页表中的地址和属性
;因为我们只是用了1MB的大小,1m/4k=256,所以只需要256个页表项
	mov ecx,256
	mov esi,0
	mov edx,PAGE_US_U|PAGE_RW_RW|PAGE_P;
.create_pte:
	mov [ebx+esi*4],edx
	add edx,4096  ;每次+0x1000。这里1MB的虚拟地址和物理地址一一对应。
	inc esi
	loop .create_pte
	
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x2000
	or eax,PAGE_US_U|PAGE_RW_RW|PAGE_P
	mov ebx,PAGE_DIR_TABLE_POS
	mov ecx,254 ;剩余内核所属的pte,未初始化数量
	mov esi,769 ;起始下标
;初始化内核其余的pde,第一个和最后一个已经初始化了,所以只有(0xc04-0xFF8)未初始化,对应的序号下标就是769-1022,总共254个
;因为虚拟地址的高1g要分配给系统
;0xC0000000虚拟地址转换后的页目录表项偏移就是 0xc00-0xfff
;所以 0xc00以下属于用户空间,而固定0xc00-0xfff之间的页目录项属于系统。
;这样不管是哪个进程只要映射0xc00-0xfff之间的页目录项都将映射内核空间。
.create_kernel_pde:
	mov [ebx+esi*4],eax
	inc esi
	add eax,0x1000
	loop .create_kernel_pde
	ret 
;---------------------------------------------------------
;            读取磁盘文件到内存
;---------------------------------------------------------	
rd_disk_m_32:
[bits 32]
;选择通道,往该通道的 Sector count 写入待操作的扇区数。
	mov dx,0x1f2
	push eax
	mov ax,cx
	out dx,al
	pop eax
;往该通道上的三个LBA寄存器写入扇区的起始地址的低24位。
	mov dx,0x1f3
	out dx,al
	
	shr eax,8
	mov dx,0x1f4
	out dx,al
	
	shr eax,8
	mov dx,0x1f5
	out dx,al
	
;往device寄存器写入LBA地址的高四位,设置LBA寻址,选择主盘或从盘。
	shr eax,0x8
    and al,0x0f 
    or al,0xe0
	and al,1110_1111b
	mov dx,0x1f6
	out dx,al
;往Command寄存器写入操作命令。
	mov dx,0x1f7
	mov al,0x20
	out dx,al
;读取status寄存器,判断硬盘工作是否完成。
.not_ready:
	in al,dx
	and al,0x8
	cmp al,0x8
	jnz .not_ready
	
;读取cx个扇区,一个扇区512字节,一次读出2个字节,读取次数 = cx*512/2 =cx*0x100
	mov eax,0x100
	mul ecx
	mov ecx,eax
	mov dx,0x1f0 ;这里有一个坑啊,因为乘积的高位会保存到dx,所以dx的赋值要留到mul指令之后
.go_on_read:
	in ax,dx
	mov word [ebx],ax
	add ebx,2
	loop .go_on_read
	ret
;---------------------------------------------------------
;            初始化内核
;---------------------------------------------------------
kerne_init:
	xor eax,eax
	xor ebx,ebx
	xor ecx,ecx
	xor edx,edx
	xor esi,esi
	xor edi,edi
	mov dx,[KERNEL_BIN_BASE_ADDR+42];program head size
	mov ebx,[KERNEL_BIN_BASE_ADDR+28];program head table offset
	mov cx,[KERNEL_BIN_BASE_ADDR+44];program head number
	
	add ebx,KERNEL_BIN_BASE_ADDR;progrom head table addr
.each_segment:
	cmp byte [ebx+0],PT_NULL;判断段类型
	je .pt_null
	mov eax,[ebx+4];p_offset
	add eax,KERNEL_BIN_BASE_ADDR;源虚拟地址
	mov esi,[ebx+16];p_filesz
	mov edi,[ebx+8];目的虚拟地址
	push esi
	push eax
	push edi
	call mem_cpy
.pt_null:
	add ebx,edx
	loop .each_segment
	ret 
;---------------------------------------------------------
;	memcopy(dst,src,copySize)
;	以字节为单位,复制内存。
;	调用约定:_stdcall
;---------------------------------------------------------
mem_cpy:
	cld ;eflag df=0
	push ebp
	push ecx
	mov ebp,esp
	
	mov edi,[ebp+0xc] ;dst
	mov esi,[ebp+0x10] ;src
	mov ecx,[ebp+0x14] ;size
	
	rep movsb
	pop ecx
	pop ebp
	ret 0xc