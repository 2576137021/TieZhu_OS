#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#define BITMAP_MASK 1
#include "stdint.h"
typedef struct _BITMAP
{
    uint32_t bitmap_byte_len;//位图的字节长度
    uint8_t* bits;//位图的字节指针
}BitMap;
/*位图初始化,使用0填充位图内存,传递bitmap结构体指针,而不是位图的字节指针*/
void bitmap_init(BitMap* pBitMap);
/*判断指定位偏移是否已占用,已占用返回1,未占用返回0,bit_offset是以0开始的位偏移*/
bool bitMap_scan_test(BitMap* pBitMap,uint32_t bit_offset);
/*在位图中申请连续的cnt位,成功返回位图起始位下标,失败返回-1*/
int bitMap_scan(BitMap* pBitMap,uint32_t cnt);
/*设置指定位的值,bit_offset是以0开始的位偏移*/
void bitmap_set(BitMap* pBitMap,uint32_t bit_offset,uint8_t value);
/*设置连续的指定位的值,bit_cnt是要设置的位长度*/
void bitmap_setEx(BitMap* pBitMap,uint32_t bit_offset,uint8_t value,uint32_t bit_cnt);
/*检查位图是否为空,为空返回true，不为空返回字节偏移*/
int32_t bitmap_isempty(BitMap* pBitMap);
#endif