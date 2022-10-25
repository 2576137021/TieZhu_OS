;简易c运行库
[bits 32]
extern main
extern exit
section .text
global _start
_start:
    ;任务入口
    push ebx ;argv
    push ecx ;argc
    call main

    ;任务退出
    push eax
    call exit