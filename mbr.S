;主引导程序
;因为blos通过jmp 0:0x7c00跳转到此处,此时cx=0,利用cx初始化其他寄存器。
;初始化栈,0x7c00以下的地址随便使用。
%include "bot.inc"
SECTION MBR vstart=0x7c00
	mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800  
    mov gs,ax 
	
	mov ax,0600h 
    mov bx,0700h
    mov cx,0
    mov dx,184fh
    int 0x10 
	
	mov eax,LOADER_START_SECTOR
	mov bx,LOADER_BASE_ADDR
	mov cx,4
	call rd_disk_m_16
	jmp LOADER_BASE_ADDR
;读取硬盘n个扇区
;eax = LBA扇区地址
;bx=数据写入的内存地址
;cx=读入的扇区数
rd_disk_m_16:
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
	mov ax,0x100
	mul cx
	mov cx,ax
	mov dx,0x1f0 ;这里有一个坑啊,因为乘积的高位会保存到dx,所以dx的赋值要留到mul指令之后
.go_on_read:
	in ax,dx
	mov [bx],ax
	add bx,2
	loop .go_on_read
	ret
	
;填充0,以补足510个字节,让0x55和0xaa在0盘0道1扇区的最后2个字节
	message db "1 MBR"
	times 510-($-$$) db 0
	db 0x55,0xaa

;10001100