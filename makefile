BUILD_DIR =./build
#kernel头文件夹
KERNEL_H_DIR =./bin/lib/kernel
#kernel文件夹
KERNEL_DIR =./bin/kernel
#device文件夹
DEVICE_DIR = ./bin/device
#kernel.bin的虚拟内存地址
ENTRY_POINT =0xc0001500
#32位文件
FILE_32BIT =-m32
#不使用c语言内建函数
FNO_BUILTIN =-fno-builtin
#nasm文件编译为elf文件
ASFLAGS =-f elf
#gcc编译			
CC = gcc
#汇编编译				
NASM = nasm $(ASFLAGS) -o 
CFLAGS =$(FILE_32BIT) $(FNO_BUILTIN) -c -W -Wstrict-prototypes -Wmissing-prototypes -fno-stack-protector
#	C编译代码	#
$(BUILD_DIR)/main.o:./bin/kernel/main.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/init.o:./bin/kernel/init.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/interrupt.o:./bin/kernel/interrupt.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/timer.o:./bin/device/timer.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/debug.o:./bin/kernel/debug.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/bitmap.o:./bin/kernel/bitmap.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/memory.o:./bin/kernel/memory.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/string.o:./bin/kernel/string.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/thread.o:./bin/thread/thread.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/list.o:./bin/kernel/list.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/sync.o:./bin/thread/sync.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/console.o:./bin/device/console.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/keyboard.o:./bin/device/keyboard.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/ioqueue.o:./bin/device/ioqueue.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/tss.o:./bin/userprog/tss.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/process.o:./bin/userprog/process.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/syscall-init.o:./bin/userprog/syscall-init.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/syscall.o:./bin/lib/use/syscall.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/stdio.o:./bin/lib/stdio.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/stdio-kernel.o:./bin/kernel/stdio-kernel.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/ide.o:./bin/device/ide.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/fs.o:./bin/fs/fs.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/file.o:./bin/fs/file.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/inode.o:./bin/fs/inode.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/dir.o:./bin/fs/dir.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/fork.o:./bin/userprog/fork.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/shell.o:./bin/shell/shell.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/buildin_cmd.o:./bin/shell/buildin_cmd.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/exec.o:./bin/userprog/exec.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/wait_exit.o:./bin/userprog/wait_exit.c
	$(CC) $(CFLAGS) -o $@ $<
$(BUILD_DIR)/pipe.o:./bin/shell/pipe.c
	$(CC) $(CFLAGS) -o $@ $<
#	汇编编译代码	#
$(BUILD_DIR)/print.o:./bin/kernel/print.S
	$(NASM)$@ $<
$(BUILD_DIR)/kernel.o:./bin/kernel/kernel.s
	$(NASM)$@ $<
$(BUILD_DIR)/switch.o:./bin/thread/switch.S
	$(NASM)$@ $<
#构建代码#
compile:$(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/print.o $(BUILD_DIR)/kernel.o \
 $(BUILD_DIR)/debug.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/string.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/switch.o \
 $(BUILD_DIR)/list.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o \
 $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/stdio-kernel.o \
 $(BUILD_DIR)/ide.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/file.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/dir.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o \
 $(BUILD_DIR)/buildin_cmd.o $(BUILD_DIR)/exec.o $(BUILD_DIR)/wait_exit.o $(BUILD_DIR)/pipe.o
 
 
link:
	ld -m elf_i386 -Ttext $(ENTRY_POINT) -e main -o kernel.bin $(BUILD_DIR)/main.o $(BUILD_DIR)/print.o $(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/kernel.o $(BUILD_DIR)/init.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o \
	$(BUILD_DIR)/string.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/switch.o $(BUILD_DIR)/list.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/console.o \
	$(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/syscall.o \
	$(BUILD_DIR)/stdio.o $(BUILD_DIR)/stdio-kernel.o $(BUILD_DIR)/ide.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/file.o $(BUILD_DIR)/dir.o \
	$(BUILD_DIR)/inode.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/buildin_cmd.o $(BUILD_DIR)/exec.o $(BUILD_DIR)/wait_exit.o \
	$(BUILD_DIR)/pipe.o 

write_disk:
	dd if=./kernel.bin of=./bin/hd60M.img bs=512 count=200 seek=9 conv=notrunc
clear:
	@cd $(BUILD_DIR) && rm -f ./*
builda:
	make clear
	make compile
	make link
	make write_disk
	sh run
