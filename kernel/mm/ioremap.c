#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>
#include <printk.h>

#define HUGE_PAGE_SIZE (0x40000000lu)
#define BIG_PAGE_SIZE (0x200000lu)
#define PAGE_SIZE (0x1000lu)

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    // alloc 1 GB page for every request to avoid reallocation
    PTE* pgdir = (PTE*) pa2kva(PGDIR_PA);
    uintptr_t phys_addr_base = ROUNDDOWN(phys_addr, HUGE_PAGE_SIZE);
    uintptr_t tmp = io_base;
    int vpn2 = get_vpn2(tmp);
    set_pfn(&pgdir[vpn2],phys_addr_base>>NORMAL_PAGE_SHIFT);
    set_attribute(&pgdir[vpn2],_PAGE_ACCESSED|_PAGE_DIRTY|_PAGE_PRESENT|_PAGE_READ|_PAGE_WRITE|_PAGE_EXEC);//a leaf node
    io_base+= HUGE_PAGE_SIZE;// 1GB
    local_flush_tlb_all();
    return (void*)(tmp + (phys_addr - phys_addr_base));
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?

    // do nothing
}
