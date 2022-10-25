#include "kerboard.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/debug.h"
#include"ioqueue.h"
#define KBD_BUF_PROT 0x60 //8042输出缓冲区
/*定义转义字符*/
#define esc '\033'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177'
/*定义不可见字符*/
#define char_invisible  0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_r_char char_invisible
#define shift_l_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible
/*定义控制字符的通码和断码*/ //有些功能按键只需要知道是否处于按下状态,所以没有断码
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define shift_r_break 0xb6 
#define shift_l_break 0xaa
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a
/*
定义一个静态二维数组
索引:通码
数组元素:按键ascll码和按键与shift结合后的ascll码
*/
static char keymap[][2]={
/* 0x00 */	{0,	0},		//占位
/* 0x01 */	{esc,	esc},		
/* 0x02 */	{'1',	'!'},		
/* 0x03 */	{'2',	'@'},		
/* 0x04 */	{'3',	'#'},		
/* 0x05 */	{'4',	'$'},		
/* 0x06 */	{'5',	'%'},		
/* 0x07 */	{'6',	'^'},		
/* 0x08 */	{'7',	'&'},		
/* 0x09 */	{'8',	'*'},		
/* 0x0A */	{'9',	'('},		
/* 0x0B */	{'0',	')'},		
/* 0x0C */	{'-',	'_'},		
/* 0x0D */	{'=',	'+'},		
/* 0x0E */	{backspace, backspace},	
/* 0x0F */	{tab,	tab},		
/* 0x10 */	{'q',	'Q'},		
/* 0x11 */	{'w',	'W'},		
/* 0x12 */	{'e',	'E'},		
/* 0x13 */	{'r',	'R'},		
/* 0x14 */	{'t',	'T'},		
/* 0x15 */	{'y',	'Y'},		
/* 0x16 */	{'u',	'U'},		
/* 0x17 */	{'i',	'I'},		
/* 0x18 */	{'o',	'O'},		
/* 0x19 */	{'p',	'P'},		
/* 0x1A */	{'[',	'{'},		
/* 0x1B */	{']',	'}'},		
/* 0x1C */	{enter,  enter},
/* 0x1D */	{ctrl_l_char, ctrl_l_char},
/* 0x1E */	{'a',	'A'},		
/* 0x1F */	{'s',	'S'},		
/* 0x20 */	{'d',	'D'},		
/* 0x21 */	{'f',	'F'},		
/* 0x22 */	{'g',	'G'},		
/* 0x23 */	{'h',	'H'},		
/* 0x24 */	{'j',	'J'},		
/* 0x25 */	{'k',	'K'},		
/* 0x26 */	{'l',	'L'},		
/* 0x27 */	{';',	':'},		
/* 0x28 */	{'\'',	'"'},		
/* 0x29 */	{'`',	'~'},		
/* 0x2A */	{shift_l_char, shift_l_char},	
/* 0x2B */	{'\\',	'|'},		
/* 0x2C */	{'z',	'Z'},		
/* 0x2D */	{'x',	'X'},		
/* 0x2E */	{'c',	'C'},		
/* 0x2F */	{'v',	'V'},		
/* 0x30 */	{'b',	'B'},		
/* 0x31 */	{'n',	'N'},		
/* 0x32 */	{'m',	'M'},		
/* 0x33 */	{',',	'<'},		
/* 0x34 */	{'.',	'>'},		
/* 0x35 */	{'/',	'?'},
/* 0x36	*/	{shift_r_char, shift_r_char},	
/* 0x37 */	{'*',	'*'},    	
/* 0x38 */	{alt_l_char, alt_l_char},
/* 0x39 */	{' ',	' '},		
/* 0x3A */	{caps_lock_char, caps_lock_char}
};
/*定义静态变量记录控制键的状态*/
static bool ctrl_status,shift_status,alt_status,ctrl_status,caps_lock_status,ext_scancode;
ioqueue keyboard_iobuffer;
/*
键盘中断处理程序
*/
static void intr_keyboard_handler(void){
    assert(get_intr_status()==INTR_OFF);
    bool break_code = false;
    bool temp_shift =shift_status;
    //读取输出端口的值,否则8042不再响应键盘中断
    uint16_t scanned_code = (uint16_t)inb(KBD_BUF_PROT);
    /*如果scanned_code是0xe0则表示会产生多个扫描码,结束当前中断接收下一个扫描码*/
    if(scanned_code==0xe0){
        ext_scancode = true;
        return;
    }
    break_code = ((scanned_code&0x80)!=0);//判断是否是断码
    if(ext_scancode){
        scanned_code = 0xe000|scanned_code;//组合双字节扫描码
    }
    ext_scancode=false; //还原多个扫描码标识变量
    if(break_code){
        uint16_t make_code = scanned_code&0x7f;//获取断码对应的通码
        if(make_code==shift_l_make){
            shift_status = false;
        }else if(make_code==alt_l_make){
            alt_status = false;
        }else if(make_code==ctrl_l_make){
            ctrl_status = false;
        }
        //非控制键断码直接退出
        return;
    }
    //判断是否是控制码
    if(scanned_code==shift_l_make||scanned_code==shift_r_make){
        shift_status = true;
        return;
    }else if(scanned_code==alt_l_make||scanned_code==alt_r_make){
        alt_status = true;
        return;
    }else if(scanned_code==ctrl_l_make||scanned_code==ctrl_r_make){
        ctrl_status = true;
        return;
    }else if(scanned_code==caps_lock_make){
        caps_lock_status =~caps_lock_status;
        return;
    }
    //组合控制码
    if(caps_lock_status==true){
        temp_shift=true;
    }
    if(shift_status==true&&caps_lock_status==true){
        temp_shift=false;
    }
    //判断是否在我们已定义的扫描码数组之内
    if(scanned_code>0&&scanned_code<0x3b)
    {
        char cur_char = 0;
        if ((scanned_code>=0x1&&scanned_code<=0xd)||scanned_code==0x1a||scanned_code==0x1b|| //这些特殊扫描码将忽略caps Lock
        scanned_code==0x27||scanned_code==0x28||scanned_code==0x29||scanned_code==0x2b||scanned_code>=0x33)
        {
           cur_char = keymap[scanned_code][shift_status];
        }else{
            cur_char = keymap[scanned_code][temp_shift];//取对应字符
        }
        //快捷键处理
        // ctr+l 和 ctrl+u 供shell使用
        if((ctrl_status && cur_char =='l') || (ctrl_status && cur_char =='u')){
            cur_char -='a';
        }
        //将字符添加到环形缓冲区
        if(cur_char){
            if(!ioq_full(&keyboard_iobuffer)){//队列未满
                //打印到屏幕上
                // put_char(cur_char);
                // put_int32(keyboard_iobuffer.head);
                //添加到队列
                ioq_putchar(&keyboard_iobuffer,cur_char);
            }
        }
    }
    //特殊处理 del键
    else
    {
        print("\nunknow make code\n");
        
    }
    return;
}
/*
键盘设备初始化
*/
void keyboard_init(void){
    //初始化键盘缓冲区
    ioqueue_init(&keyboard_iobuffer);
    //注册中断例程
    register_handler(0x21,intr_keyboard_handler);
    print("keyboard_init success\n");
}
