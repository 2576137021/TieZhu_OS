
[bits 32]
section .text
global switch_to
;switch_to 是根据线程或进程的内核栈来确定返回位置的
switch_to:
    ;retVaddr
    push esi
    push edi
    push ebx
    push ebp 

    mov eax,[esp+20] ;  arg1 cur_thread
    mov [eax],esp    ;  保存当前esp到cur_thread的self_stack
    
    mov eax,[esp+24];   arg2 next_thread_pcb
    mov esp,[eax];      修改esp指向next_thread的self_stack

    pop ebp
    pop ebx
    pop edi
    pop esi
    ret                 ;如果进程是第一次运行那么,此时的esp指向kernel_thread,如果之前已经指向过这个线程,那么esp指向的是上一次switch_to保存esp,指向retVaddr地址,