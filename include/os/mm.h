/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+PAGE_SIZE)

#define PAGE_TABLE_SIZE 4096 // 4K
#define INIT_KERNEL_PAGETABLE   0xffffffc054000000
#define SWAP_INFO_PLACE         0xffffffc055000000 //存放swap信息的地址
#define VA_INFO_PLACE           0xffffffc055500000 //存放VA信息的地址
#define TEMP_TASK_IMG           0xffffffc056000000 //暂时存放task内容的地址
#define BLOCK_MAP_PLACE         0xffffffc056500000 //存放block map信息的地址
#define INODE_PLACE             0xffffffc057000000 //存放inode信息的地址
#define TEMP                    0xffffffc057500000 //暂时存放内容的地址
#define TEMP2                   0xffffffc057600000 //暂时存放内容的地址
#define TEMP3                   0xffffffc057700000 //暂时存放内容的地址
#define TEMP4                   0xffffffc057800000 //暂时存放内容的地址


/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

#define MAP_PAGE_ACCESSED 0x80

typedef struct swap_
{
    reg_t va;
    uint8_t asid;

    //notice:the real sec num is padding_start_sec+sec_num*8
    int sec_num;    
} swap_info_t;

void init_mm();

extern ptr_t allocPage(uint8_t asid);
extern ptr_t allocPagetable(uint8_t asid);
// TODO [P4-task1] */
void freePage(ptr_t baseAddr);
void freeProcessMem(uint8_t asid);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

// TODO [P4-task1] */
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir,uint8_t asid);

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);

//find the page in swap
//return the start sector of the page in swap, 0 if not in swap
swap_info_t* page_in_swap(reg_t va,reg_t asid);

//choose a page to swap out
ptr_t choose_swap();

//pgdir in pa
//return PTE* if page is in mem, 0 otherwise
PTE* get_pte(reg_t va,reg_t pgdir);

void set_map_page_accessed(int index);

void swap_in(reg_t kva,swap_info_t* info);

static inline uint64_t get_kva(uintptr_t va, uintptr_t pgdir)
{
    PTE* pte = get_pte(va, pgdir);
    return pa2kva(get_pa(*pte)) + get_page_offset(va);
}


#endif /* MM_H */
