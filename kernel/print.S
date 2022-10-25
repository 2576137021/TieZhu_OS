;显存格式 80*25 = 2000个字符,一行80个字符,一共25行
SELECTOR_VIDEO equ 0x3<<3 
put_int32_buffer dq 0,0
flag db 0
hex_prefix db "0x",0
[bits 32]
global put_char
global print
global put_int32
global set_cursor
global get_eip
global cls_screen
global get_cursor

;参数 数字(未转换为ascll文本)32位
put_int32:
    push eax
    push ecx
    push edx
    push ebx
    push esi
    push edi
    xor edi,edi
    xor ebx,ebx
    xor edx,edx
    xor esi,esi 
    mov byte [flag],0
    mov dword [put_int32_buffer],0
    mov dword [put_int32_buffer+4],0
    mov ecx,4
    mov edi,7
    lea eax,[esp+4+4+4+4+4+4+4+3] ;小端序 得到字节最高位  0x64 100 

    .loop_put_int32:
    mov dl,byte [eax]       ;从最高位开始打印输出
    shr dl,4                 ;取出一个字节的高4位

    .judge:
    cmp esi,2
    je .call_print_putint32
    inc esi ;一个字节处理完毕

    ;高位去除连续的0
    cmp byte [flag],0
    jnz .no_zero_put
    cmp dl,0
    jne .set_flag
    jmp .judge_low4
    .set_flag:
    mov byte [flag],1
    jmp .no_zero_put

    .no_zero_put:
    ;设置读写buffer的指针
    mov ebx,put_int32_buffer+7 ;指向buffer
    sub ebx,edi;当前buffer指针
    dec edi

    cmp dl,9
    ja .put_int_A_F
    
    ;如果是1-9的数字
    add dl,48
    mov byte [ebx],dl   ;每一个16进制都要使用一个字节的ascll码表示,所以4字节的整数就需要8+1字节的buffer,最后一个是结尾符0
    jmp .judge_low4

    ;如果是A-F的数字
    .put_int_A_F:
    add dl,55
    mov byte [ebx],dl 

    ;判断低4位
    .judge_low4:
    mov dl,byte [eax]
    and dl,0xf
    jmp .judge

    .call_print_putint32:
    xor esi,esi
    dec eax
    loop .loop_put_int32

    ;调用print 打印字符串
    push hex_prefix
    call print
    add esp,4

    push put_int32_buffer
    call print
    add esp,4

    pop edi
    pop esi
    pop ebx
    pop edx
    pop ecx
    pop eax
    mov dword [esp+4],0
    ret 

print:
    push ecx
    push eax
    xor ecx,ecx
    mov eax,[esp+4+4+4] ;得到字符串地址
    .loop_print:
    mov cl,byte [eax] ;得到第一个字符
    cmp cl,0
    jz exit_print

    push ecx
    call put_char
    add esp,4

    inc eax
    jmp .loop_print
    exit_print:
    pop eax
    pop ecx
    ret 
;栈顶存放输出的字符
put_char:
    pushad
    ;初始化显存段选择子
    mov ax,SELECTOR_VIDEO
    mov gs,ax
    ;获取光标高8位
    xor eax,eax
    mov dx,0x3d4
    mov al,0xe
    out dx,al
    mov dx,0x3d5
    in al,dx
    mov ah,al
    ;获取光标位置低8位
    mov dx,0x3d4
    mov al,0xf
    out dx,al
    mov dx,0x3d5
    in al,dx
    ;将光标位置传给bx寄存器
    mov bx,ax

    ;获取要输出的字符
    mov ecx,[esp+32+4] ;pushad = 8*4 Byte,return addr = 4 Byte
    
    ;判断是否是特殊字符
    cmp cl,0xd
    jz .call_is_carriage_return
    cmp cl,0xa
    jz .call_is_line_feed
    cmp cl,0x8
    jz .call_is_backspace
    ;其他字符打印
    call .put_other
    jmp .return_putchar

;---------------------------------------------------------------------------------------
;                                                                                      
;                                               子功能区                                
;                                                                                      
;---------------------------------------------------------------------------------------


;---------------------------------------------------------------------------
;功能:功能区跳转
;实现思路
;---------------------------------------------------------------------------

;外部调用 调用后自动退出put_char
.call_is_line_feed:
    call .is_line_feed
    jmp .return_putchar
.call_is_backspace:
    call .is_backspace
    jmp .return_putchar
.call_is_carriage_return:
    call .is_carriage_return
    jmp .return_putchar
.return_putchar:
    popad
    ret 
;--------------------------------------------------------
;功能:设置光标位置
;实现思路:进入此方法前 bx存放当前光标位置。
;--------------------------------------------------------
.set_cursor:
    push eax
    push edx
    ;设置光标高8位
    mov dx,0x3d4
    mov al,0xe
    out dx,al
    mov dx,0x3d5
    mov al,bh
    out dx,al
    ;设置光标低8位
    mov dx,0x3d4
    mov al,0xf
    out dx,al
    mov dx,0x3d5
    mov al,bl
    out dx,al
    pop edx
    pop eax
    ret

;---------------------------------------------------------------------------
;功能:滚动屏幕
;实现思路:将当前显示的第2行到25行,移动到显存的对应的第1行到第24行。将最后一行清空。
;---------------------------------------------------------------------------
.roll_screen:
    push ecx
    cld
    mov ecx,960  ;需要复制 (2000-80)*2/4 = 960 次
    ;移动  不使用rep movsd 因为它会使用默认寄存器 es和ds
    mov esi,160             ;复制 2-25 到 1-24
    mov edi,0
    .copy_roll_screen:
    mov eax,dword [gs:esi]
    mov dword [gs:edi],eax
    add esi,4
    add edi,4
    loop .copy_roll_screen
    ;清空最后一行
    xor edx,edx
    mov dx,3840 ;80*24*2
    mov ecx,80
    .clear_last_column:
    mov word [gs:edx],0x0720;
    add edx,2
    loop .clear_last_column
    shr edx,1
    mov ebx, 1920;3840/2
    call .set_cursor
    pop ecx
    ret 
;--------------------------------------------------------
;功能:显示一个字符
;实现思路:当前光标位置处写入字符和字符格式,移动光标到下一个位置。
;判断是否需要换页
;--------------------------------------------------------
.put_other:
    cmp bx,2000
    jb .no_roll_screen
    call .roll_screen ;滚动屏幕
    .no_roll_screen:
    shl bx,1
    mov byte [gs:bx],cl
    mov byte [gs:bx+1],0x7
    shr bx,1
    add bx,1
    cmp bx,2000
    jl .put_other_no_change_page ;小于2000则不滚动屏幕
    call .roll_screen
    .put_other_no_change_page:
    call .set_cursor
    ret 
;--------------------------------------------------------
;功能:换行
;实现思路:\n 光标移动到行首,再移动到下一行  bx传递字符位置
;判断是否需要换页
;--------------------------------------------------------
.is_line_feed:
    call .is_carriage_return
    add bx,80
    cmp bx,2000
    jl .is_line_feed_ret
    call .roll_screen
.is_line_feed_ret:
    call .set_cursor
    ret
;--------------------------------------------------------
;功能:回车
;实现思路:;\r 回车就是移动到行首
;判断是否需要换页
;--------------------------------------------------------
.is_carriage_return:
    xor edx,edx  ;dx是被除数的高16位所以要清空
    mov ax,bx
    mov si,80

    div si ;ax行数 dx是基于行的字符偏移  debug address:0008:0xc00016cf
           ;这里不需要计算字符的内存偏移,因为更新坐标本身就是使用的字符编译
    sub ebx,edx;回到行首
    call .set_cursor
    ret
;--------------------------------------------------------
;功能:退格键
;实现思路:将当前光标位置的显存两个字节的数据清空,移动光标到前一个位置
;判断是否需要换页,是否是第一页的第一个位置。
;bx存储当前光标位置
;--------------------------------------------------------
.is_backspace:

    dec bx      ;当前光标位置的前一个字符
    shl bx,1    ;左移一位 =光标位置*2 =当前字符在显存的地址偏移 

    mov byte [gs:bx],0;清空当前字符和字符格式
    mov byte [gs:bx+1],0

    shr bx,1
    inc bx      ;还原光标位置

    cmp bx,0;是否是第一页第一个
    je .is_backspace_retn
    dec bx ;记录移动后光标位置
    call .set_cursor ;移动光标函数
    .is_backspace_retn:
    ret 
;--------------------------------------------------------
;功能:设置光标位置(外部使用)
;实现思路:进入此方法前 [esp] = 当前光标位置。
;--------------------------------------------------------
set_cursor:
    mov bx,word [esp+4]
    push eax
    push edx
    ;设置光标高8位
    mov dx,0x3d4
    mov al,0xe
    out dx,al
    mov dx,0x3d5
    mov al,bh
    out dx,al
    ;设置光标低8位
    mov dx,0x3d4
    mov al,0xf
    out dx,al
    mov dx,0x3d5
    mov al,bl
    out dx,al
    pop edx
    pop eax
    mov dword [esp+4],0
    ret

;--------------------------------------------------------
;功能:获取当前EIP(debug使用)
;--------------------------------------------------------
get_eip:
    mov eax,[esp]
    ret

;--------------------------------------------------------
;功能:清除屏幕内容
;--------------------------------------------------------
cls_screen:
    pushad
    ;给gs赋值，用户进程提权后gs=0
    mov ax,SELECTOR_VIDEO
    mov gs,ax

    mov ebx,0
    mov ecx,80*25

    ;使用黑底白字覆盖全屏幕
.cls:
    mov word [gs:ebx],0x0720
    add ebx,2
    loop .cls

    push 0
    call set_cursor
    add esp,4
    
    popad
    ret 
;--------------------------------------------------------
;功能:获取光标位置(外部使用)
;--------------------------------------------------------
get_cursor:
    ;初始化显存段选择子
    mov ax,SELECTOR_VIDEO
    mov gs,ax
    ;获取光标高8位
    xor eax,eax
    mov dx,0x3d4
    mov al,0xe
    out dx,al
    mov dx,0x3d5
    in al,dx
    mov ah,al
    ;获取光标位置低8位
    mov dx,0x3d4
    mov al,0xf
    out dx,al
    mov dx,0x3d5
    in al,dx
    ;ax中存放光标位置
    mov dword esi,[esp+0x4]
    mov dword [esi],eax
    ret