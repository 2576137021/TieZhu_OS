if [[ ! -d "../lib" || ! -d "../../build" ]];then
    echo "dependent dir don't exist"
    cwd=${pwd}
    cwd=${cwd##*/}
    cwd=${cwd%/}
    if [[ $cwd != "command" ]];then
        echo -e "you\'d better in command dir\n"
    fi
    exist
fi

BIN="prog_pipe"
CFLAGS="-Wall -c -fno-builtin -W -Wstrict-prototypes \
-Wmissing-prototypes -Wsystem-headers -fno-stack-protector"
LIBS="-I ../lib -I ../lib/user ../lib/kernel"
OBJS="../../build/print.o ../../build/string.o \
../../build/debug.o  ../../build/stdio.o ../../build/syscall.o ../../build/fork.o ../../build/pipe.o start.o"
DD_IN=$BIN
DD_OUT="/home/admin1/bochs/bin/hd60M.img"

##编译
nasm -f elf ./start.s -o ./start.o
ar rcs simple_crt.a $OBJS start.o
gcc -m32 -fno-builtin $CFLAGS $LIBS -o $BIN".o" $BIN".c"

ld -m elf_i386 -o $BIN $BIN".o" simple_crt.a
SEC_CNT=$(ls -l $BIN|awk '{printf("%d",($5+511)/512)'})

if [[ -f $BIN ]];then
    dd if=./$DD_IN of=$DD_OUT bs=512 \
    count=$SEC_CNT seek=300 conv=notrunc
fi


