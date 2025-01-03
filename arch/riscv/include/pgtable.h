#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)

#define PTE_MASK ((1lu<<54)-1)

#define PPN_MASK (((1lu<<44) - 1) << 10)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    /* TODO: [P4-task1] */
    return (kva & 0xffffffff);
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    /* TODO: [P4-task1] */
    return (pa | 0xffffffc000000000lu);
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    return ((entry & PTE_MASK) >> _PAGE_PFN_SHIFT);
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    reg_t old_entry = ((*entry) & (~PPN_MASK));
    pfn = ((pfn << _PAGE_PFN_SHIFT) & PTE_MASK);
    *entry =old_entry| pfn;
}



static inline void clean_pte(PTE *entry)
{
    *entry = 0;
}


/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    /* TODO: [P4-task1] */
    return (get_pfn(entry) << NORMAL_PAGE_SHIFT);
}


/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    /* TODO: [P4-task1] */
    return (entry & mask);
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    *entry |= bits;
}

static inline void unset_attribute(PTE *entry, uint64_t bits)
{
    *entry &= ~bits;
}

static inline uint64_t get_page_base(uint64_t vaddr)
{
    return (vaddr & ~(NORMAL_PAGE_SIZE-1));
}

static inline uint64_t get_page_offset(uint64_t vaddr)
{
    return (vaddr & (NORMAL_PAGE_SIZE-1));
}

static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    /* TODO: [P4-task1] */
    int num = NORMAL_PAGE_SIZE / sizeof(uint64_t);
    uint64_t* p = (uint64_t*)pgdir_addr;
    for(int i=0; i<num ; i++)
    {
        *p = 0;
        p++;
    }
}

static inline uint64_t get_vpn2(uint64_t vaddr)
{
    return (vaddr & VA_MASK)>>(NORMAL_PAGE_SHIFT+PPN_BITS+PPN_BITS);
}

static inline uint64_t get_vpn1(uint64_t vaddr)
{
    return ((vaddr&VA_MASK)>>(NORMAL_PAGE_SHIFT+PPN_BITS))&((1lu<<(PPN_BITS))-1);
}

static inline uint64_t get_vpn0(uint64_t vaddr)
{
    return ((vaddr & VA_MASK) >> (NORMAL_PAGE_SHIFT))&((1lu << (PPN_BITS)) - 1);
}


/* 
 * query the page table stored in pgdir_va to obtain the physical 
 * address corresponding to the virtual address va.
 * 
 * return the kernel virtual address of the physical address 
 */
/*
static inline uintptr_t get_kva_of(uintptr_t va, uintptr_t pgdir_va)
{
    // TODO: [P4-task1] (todo if you need)
}
*/

#endif  // PGTABLE_H
