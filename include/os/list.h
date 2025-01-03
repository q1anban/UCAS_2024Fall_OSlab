/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Copyright (C) 2018 Institute of Computing
 * Technology, CAS Author : Han Shukai (email :
 * hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Changelog: 2019-8 Reimplement queue.h.
 * Provide Linux-style doube-linked list instead of original
 * unextendable Queue implementation. Luming
 * Wang(wangluming@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * */

#ifndef INCLUDE_LIST_H_
#define INCLUDE_LIST_H_

#include <type.h>

// double-linked list
typedef struct list_node
{
    struct list_node *next, *prev;
} list_node_t;

typedef list_node_t list_head;

// LIST_HEAD is used to define the head of a list.
#define LIST_HEAD(name) struct list_node name = {&(name), &(name)}

/* TODO: [p2-task1] implement your own list API */

/*note : API below should be used with pointer instead of struct list_node */
#define LIST_IS_EMPTY(head) ((head)->next == (head))

static inline int list_is_empty(list_node_t* node)
{
    return (node->next == node);
}

#define NODE_TO_PCB(node) ((pcb_t*)((reg_t)node - 4 * sizeof(reg_t)))

#define LIST_ADD_HEAD(head, node) do { \
    (node)->next = (head)->next; \
    (node)->prev = (head); \
    (head)->next->prev = (node); \
    (head)->next = (node); \
} while (0)

#define LIST_ADD_TAIL(head, node) do { \
    (node)->next = (head); \
    (node)->prev = (head)->prev; \
    (head)->prev->next = (node); \
    (head)->prev = (node); \
} while (0)

#define LIST_REMOVE(node) do { \
    (node)->prev->next = (node)->next; \
    (node)->next->prev = (node)->prev; \
} while (0)

#define NODE_REFRESH(node) do { \
    (node)->next = (node); \
    (node)->prev = (node); \
} while (0)
static inline void node_refresh(list_node_t *node)
{
    node->next = node;
    node->prev = node;
}
#endif
