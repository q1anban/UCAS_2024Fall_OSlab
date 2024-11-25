#include <os/mm.h>

#define FREE_MEM_SIZE (INIT_KERNEL_PAGETABLE - INIT_KERNEL_STACK)
#define FREE_PGT_SIZE (TEMP_TASK_IMG - INIT_KERNEL_PAGETABLE)

#define MEM_PAGE_NUM (FREE_MEM_SIZE/PAGE_SIZE)
#define PGT_PAGE_NUM (FREE_PGT_SIZE/PAGE_SIZE)
// NOTE: A/C-core
static ptr_t kernMemCurr = INIT_KERNEL_STACK;
static ptr_t kernPgTCurr = INIT_KERNEL_PAGETABLE;
//using bit map to manage the free memory
uint8_t free_mem_map[MEM_PAGE_NUM];
uint8_t free_pgt_map[PGT_PAGE_NUM];
int mem_ptr=0;
int pgt_ptr=0;
//restriced function used to implement clock algorithm

static inline void clean_page(uint64_t* page)
{
    int num = PAGE_SIZE / sizeof(uint64_t);
    for(int i=0;i<num;i++)
    {
        *page = 0;
        page++;
    }
}


void init_mm()
{
    //clear memory map and page table map
    for (int i = 0; i < MEM_PAGE_NUM; i++)
        free_mem_map[i] = 0;
    for(int i=0;i<PGT_PAGE_NUM;i++)
        free_pgt_map[i] = 0;
}



ptr_t allocPage(uint8_t asid)
{
    for(int i=0;i<MEM_PAGE_NUM;i++)
    {
        
        if(free_mem_map[i]==0)
        {
            free_mem_map[i] = asid;
            //clear the memory
            ptr_t ret = INIT_KERNEL_STACK + i * PAGE_SIZE;
            clean_page((uint64_t*)ret);
            return ret;       
        }
    }
    //no more free memory ,swap out some page
    return 0;
}

ptr_t allocPagetable(uint8_t asid) 
{
    for(int i=0;i<PGT_PAGE_NUM;i++)
    {
        if(free_pgt_map[i]==0)
        {
            free_pgt_map[i] = asid;
            reg_t ret = INIT_KERNEL_PAGETABLE + i * PAGE_SIZE;
            clean_page((uint64_t*)ret);
            return ret;
        }
    }
    return 0;
}

reg_t page_in_swap(reg_t va,reg_t asid)
{
    return 0;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif



void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
}

void freeProcessMem(uint8_t asid)
{
    //free mem and pgt that belong to the given asid
    for(int i=0;i<MEM_PAGE_NUM;i++)
    {
        if(free_mem_map[i]==asid)
        {
            free_mem_map[i] = 0;
        }
    }
    for(int i=0;i<PGT_PAGE_NUM;i++)
    {
        if(free_pgt_map[i]==asid)
        {
            free_pgt_map[i] = 0;
            clean_page((uint64_t*)(INIT_KERNEL_PAGETABLE + i * PAGE_SIZE));
        }
    }
}


void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
//function: copy the mapping in src_pgdir to dest_pgdir
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    int num = PAGE_SIZE / sizeof(PTE);
    PTE* dest = (PTE*)dest_pgdir;
    PTE* src = (PTE*)src_pgdir;
    for(int i=0;i<num;i++)
    {

        dest[i] = src[i];
    }
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
//using small page : 4KB
//we assume that alloc and free operation have cleared what they allocated
//receive pgdir as kva
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir,uint8_t asid)
{
    // TODO [P4-task1] alloc_page_helper:
    va &= VA_MASK;
    uintptr_t kva = allocPage(asid);
    // map the page into pgdir
    uint64_t vpn2 = get_vpn2(va);
    PTE* pgd0 = (PTE*)pgdir;
    if(pgd0[vpn2]==0)
    {
        uintptr_t pgt = allocPagetable(asid);//kva
        set_pfn(&pgd0[vpn2],kva2pa(pgt)>>NORMAL_PAGE_SHIFT);
        set_attribute(&pgd0[vpn2],_PAGE_PRESENT);//other bits are set to 0
    }
    PTE* pgd1 =pa2kva(get_pa(pgd0[vpn2]));
    uint64_t vpn1 = get_vpn1(va);
    if(pgd1[vpn1]==0)
    {
        uintptr_t pgt = allocPagetable(asid);
        set_pfn(&pgd1[vpn1],kva2pa(pgt)>>NORMAL_PAGE_SHIFT);
        set_attribute(&pgd1[vpn1],_PAGE_PRESENT);
    }
    PTE* pgd2 = pa2kva(get_pa(pgd1[vpn1]));
    uint64_t vpn0 = get_vpn0(va);
    set_pfn(&pgd2[vpn0],kva2pa(kva)>>NORMAL_PAGE_SHIFT);
    set_attribute(&pgd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER);
    return kva;
}

reg_t page_in_mem(reg_t va,reg_t pgdir)
{
    //pgdir is pa
    PTE* pgd0 = (PTE*)pa2kva(pgdir);
    int vpn2 = get_vpn2(va);
    int vpn1 = get_vpn1(va);
    int vpn0 = get_vpn0(va);
    if(get_attribute(pgd0[vpn2],_PAGE_PRESENT)==0)
        return 0;
    PTE* pgd1 = (PTE*)pa2kva(get_pa(pgd0[vpn2]));
    if(get_attribute(pgd1[vpn1],_PAGE_PRESENT)==0)
        return 0;
    PTE* pgd2 = (PTE*)pa2kva(get_pa(pgd1[vpn1]));
    if(get_attribute(pgd2[vpn0],_PAGE_PRESENT)==0)
        return 0;
    return (reg_t)pgd2 + sizeof(PTE)*vpn0;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}