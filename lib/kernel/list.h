#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "stdint.h"
/*
通过链表元素获取包含链表结构体的首地址
*/
#define strcut_offset(struct_name,elem_name)    (uint32_t)(&((struct_name* )0)->elem_name)
/*
功能:获取链表节点所属的结构体起始地址
elem_ptr:链表元素地址
struct_name:包含链表的结构体名称
elem_name：链表结构体中的元素名称
返回值:链表节点所属的结构体起始地址
*/
#define struct_entry(elem_ptr,struct_name,elem_name) (struct_name*)((uint32_t)elem_ptr-strcut_offset(struct_name,elem_name))
/*
链表节点
*/
typedef struct _list_elem
{
    struct _list_elem* prev;
    struct _list_elem* next;
}list_elem, * pList_elem;
/*
双向链表
*/
typedef struct _list
{
    list_elem head; //头节点
    list_elem tail; //尾节点
}list, * pList;
/*
自定义函数类型,用作list_traversal的回调函数
*/
typedef bool functionc(list_elem* elem, void* arg);
/*
功能:初始化双向链表
*/
void list_init(list* list);
/*
功能:将链表元素elem插入到链表元素before之前
*/
void list_insert_before(list_elem* before, list_elem* elem);
/*将链表元素添加到头部*/
void list_push(list* list, list_elem* elem);
/*将链表元素添加到尾部*/
void list_append(list* list, list_elem* elem);
/*使链表元素脱离链表*/
void list_remove(list_elem* elme);
/*将链表第一个元素弹出并返回*/
list_elem* list_pop(list* list);
/*从链表中查找元素 elem,成功返回true,失败返回false*/
bool list_elem_find(list* list, list_elem* elem);
/*返回链表的长度*/
uint32_t list_len(list* list);
/*判断链表是否为空,为空返回TRUE否则返回FALSE*/
bool list_isempty(list* list);
/*
功能:遍历链表,通过向func传入链表元素与arg,如果满足func的条件则返回当前元素。
返回：失败返回NULL,成功返回链表元素节点
*/
list_elem* list_traversal(list* list, functionc* func, void* arg);
/**
 * @brief 
 * 遍历list,返回元素数量
 */
uint32_t list_each(list* list);


#endif