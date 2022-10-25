#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/string.h"
#include "../lib/kernel/debug.h"
void bitmap_init(BitMap* pBitMap){
    memset(pBitMap->bits,0,pBitMap->bitmap_byte_len);
    
}
bool bitMap_scan_test(BitMap* pBitMap,uint32_t bit_offset){
    uint32_t byte_offset = bit_offset/8;
    bit_offset = bit_offset%8;
    return (pBitMap->bits[byte_offset] & (BITMAP_MASK<<bit_offset))>>bit_offset;

}
int bitMap_scan(BitMap* pBitMap,uint32_t cnt){
     uint32_t byte_idx = 0;
    while (pBitMap->bits[byte_idx] == 0xFF && byte_idx < pBitMap->bitmap_byte_len)//找到存在空位的字节偏移处
    {
        byte_idx++;
    }
    //找到连续未被占用的位偏移
    uint8_t bit_idx = -1;
    //记录连续空闲的位数
    uint8_t leisure_cnt = 0;
    //一个字节内的位偏移
    uint8_t bit_offset = 0;
    while (byte_idx < pBitMap->bitmap_byte_len)
    {
        bool flag = pBitMap->bits[byte_idx] & (BITMAP_MASK << bit_offset);
        //判断是否达到需要的连续空位长度
        if (leisure_cnt == cnt) {
            //找到后跳出
            bit_idx = byte_idx * 8 + bit_offset-cnt;
            break;
        }
        if (flag)leisure_cnt = 0;else leisure_cnt++;
        bit_offset++;
        if (bit_offset == 8) {
            bit_offset = 0;
            byte_idx++;
        }
    }
    return bit_idx;
}
void bitmap_set(BitMap* pBitMap,uint32_t bit_offset,uint8_t value){
    uint32_t byte_offset = bit_offset / 8;
    bit_offset = bit_offset % 8;
    if (value == 0) {
        pBitMap->bits[byte_offset] &= ~(BITMAP_MASK << bit_offset);
    }
    else {
        pBitMap->bits[byte_offset] |= (value << bit_offset);
    }
    return;
}
void bitmap_setEx(BitMap* pBitMap,uint32_t bit_offset,uint8_t value,uint32_t bit_cnt){

    while (bit_cnt--)
    {
        bitmap_set(pBitMap,bit_offset,value);
        bit_offset++;
    }
    
}
/*检查位图是否为空,为空返回true，不为空返回字节偏移*/
int32_t bitmap_isempty(BitMap* pBitMap){
    assert(pBitMap!=NULL);
    int32_t len =0;
    while(len<(int32_t)pBitMap->bitmap_byte_len){
        if(*(pBitMap->bits+len)!=0){
            return len;
        }
        len++;
    }
    return true;
}