#include "../lib/kernel/list.h"
#include "../lib/kernel/interrupt.h"
/*
功能:初始化双向链表
*/
void list_init(list* list) {
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.next = NULL;
    list->tail.prev = &list->head;
}
/*
功能:将链表元素elem插入到链表元素before之前
*/
void list_insert_before(list_elem* before, list_elem* elem) {
    //禁止可屏蔽中断
    enum intr_status status = intr_disable();
    elem->prev = before->prev;
    elem->next = before;
    before->prev->next = elem;
    before->prev = elem;
    intr_set_status(status);
}
/*将链表元素添加到头部*/
void list_push(list* list, list_elem* elem) {
    list_insert_before(list->head.next, elem);
}
/*将链表元素添加到尾部*/
void list_append(list* list, list_elem* elem) {
    list_insert_before(&list->tail, elem);
}

/*使链表元素脱离链表*/
void list_remove(list_elem* elme) {
    enum intr_status status = intr_disable();
    elme->next->prev = elme->prev;
    elme->prev->next = elme->next;
    intr_set_status(status);
}
/*将链表第一个元素弹出并返回*/
list_elem* list_pop(list* list) {
    list_elem* elem = list->head.next;
    list_remove(elem);
    return elem;
}
/*从链表中查找元素 elem,成功返回true,失败返回false*/
bool list_elem_find(list* list, list_elem* elem) {
    enum intr_status status = intr_disable();
    list_elem* now_elem = list->head.next;
    while (now_elem != &list->tail)
    {
        
        if (now_elem == elem) {
            return true;
        }
        now_elem = now_elem->next;
    }
    intr_set_status(status);
    return false;
}
/*返回链表的长度*/
uint32_t list_len(list* list) {
    uint32_t len = 0;
    list_elem* now_elem = list->head.next;
    while (now_elem != &list->tail)
    {
        len++;
        now_elem = now_elem->next;
    }
    return len;
}
/*判断链表是否为空,为空返回TRUE否则返回FALSE*/
bool list_isempty(list* list) {
    return (list->head.next == &list->tail ? true : false);
}
/*
功能:遍历链表,通过向func传入链表元素与arg,如果满足func的条件则返回当前元素。
返回：失败返回NULL,成功返回链表元素节点
*/
list_elem* list_traversal(list* list, functionc* func, void* arg) {
    list_elem* now_elem = list->head.next;
    if (list_isempty(list)) {
        return NULL;
    }
    while (now_elem != &list->tail)
    {
        if (func(now_elem, arg)) {
            return now_elem;
        }
        now_elem = now_elem->next;
    }
    return NULL;
}
/**
 * @brief 
 * 遍历list,返回元素数量
 */
uint32_t list_each(list* list){
    list_elem* now_elem = list->head.next;
    uint32_t cnt;
    if (list_isempty(list)) {
        return 0;
    }
    while (now_elem != &list->tail)
    {
        cnt++;
        now_elem = now_elem->next;
    }
    return cnt;
}