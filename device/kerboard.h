#ifndef DEVICE_KEYBOARD_H
#define DEVICE_KEYBOARD_H
#include"ioqueue.h"
void keyboard_init(void);
/*
键盘缓冲区
*/
extern ioqueue keyboard_iobuffer;

#endif