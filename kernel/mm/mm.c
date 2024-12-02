#include <os/mm.h>
#include <os/sched.h>
#include <common.h>
#include <os/task.h>
#include <printk.h>

#define FREE_MEM_SIZE (INIT_KERNEL_PAGETABLE - INIT_KERNEL_STACK)
#define FREE_PGT_SIZE (TEMP_TASK_IMG - INIT_KERNEL_PAGETABLE)

//macro for swap 
#define SWAP_INFO_NUM 4096
#define SEC_INFO_NUM 4096

#define MEM_PAGE_NUM (FREE_MEM_SIZE/PAGE_SIZE)
#define PGT_PAGE_NUM (FREE_PGT_SIZE/PAGE_SIZE)
// NOTE: A/C-core
static ptr_t kernMemCurr = INIT_KERNEL_STACK;
static ptr_t kernPgTCurr = INIT_KERNEL_PAGETABLE;
//using bit map to manage the free memory
uint8_t free_mem_map[MEM_PAGE_NUM];
uint8_t free_pgt_map[PGT_PAGE_NUM];
int clock_ptr=0;
int sec_ptr=0;

swap_info_t *swap_info = (swap_info_t*)SWAP_INFO_PLACE;

//notice : kernel page will not have a va 
uint64_t va_info[MEM_PAGE_NUM] ;// used to store the va of each KVA

extern int padding_start_sec;//the first sector of padding

static inline int is_same_proc(uint8_t flag,uint8_t asid)
{
    return ((flag&asid)==asid);
}

static inline void clean_page(uint64_t* page)
{
    int num = PAGE_SIZE / sizeof(uint64_t);
    for(int i=0;i<num;i++)
    {
        *page = 0;
        page++;
    }
}



//swap out a page specified by kva
void swap_out(reg_t kva)
{
    //fit swap info
    int i=0;
    int num = (kva - INIT_KERNEL_STACK) / PAGE_SIZE;
    reg_t va = get_page_base(va_info[num]);
    reg_t asid = free_mem_map[num];
    while(swap_info[i].asid!=0)//find an empty swap info
    {
        i++;
    }
    
    swap_info[i].va = va;
    swap_info[i].asid = asid;
    reg_t pgdir=0;
    for(int j=0;j<TASK_MAXNUM;j++)
    {
        if(pcb[j].asid==asid && pcb[j].status != TASK_EXITED)
        {
            pgdir = pcb[j].satp;
            break;
        }
    }
    
    //remove the mapping in the swapped out pgdir
    //just set the PRESENT bit to 0
    PTE* pte = get_pte(va,pgdir);
    if(pte)//mysterious bug here ,*pte should never be null
    {
        reg_t ppn = get_pa(*pte);
        unset_attribute(pte,_PAGE_PRESENT);
    }
    free_mem_map[num] = 0;
    va_info[num] = 0;


    swap_info[i].sec_num = sec_ptr;

    sec_ptr++;

    sd_write(kva2pa(kva),PAGE_SIZE/SECTOR_SIZE,padding_start_sec+swap_info[i].sec_num*8);

    printk("swap out page based on %x, used to map %x, asid %d, sec_num %d\n",kva,va,asid,swap_info[i].sec_num);
}

//swap in a page specified by swap info to kva
void swap_in(reg_t kva,swap_info_t* info)
{
    //read from swap
    sd_read(kva2pa(kva),PAGE_SIZE/SECTOR_SIZE,padding_start_sec+info->sec_num*8);

    printk("swap in page based on %x, used to map %x, asid %d, sec_num %d\n",kva,info->va,info->asid,info->sec_num);
    int num = (kva - INIT_KERNEL_STACK) / PAGE_SIZE;
    //config free_mem_map
    free_mem_map[num] = info->asid;
    //config va_info
    va_info[num] = info->va;
    //clear swap info
    info->va = 0;
    info->asid = 0;
    info->sec_num = 0;
}

void init_mm()
{
    //clear memory map and page table map
    for (int i = 0; i < MEM_PAGE_NUM; i++)
    {
        free_mem_map[i] = 0;
        va_info[i] = 0;
    }
    for(int i=0;i<PGT_PAGE_NUM;i++)
        free_pgt_map[i] = 0;
    
    
    //clean swap info and sec info
    for(int i=0;i<SWAP_INFO_NUM;i++)
    {
        swap_info[i].va = 0;
        swap_info[i].asid = 0;
        swap_info[i].sec_num = 0;
    }

}



ptr_t allocPage(uint8_t asid)
{
    int i;
    for(i=0;i<MEM_PAGE_NUM;i++)
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
    reg_t kva = choose_swap();
    int num = (kva - INIT_KERNEL_STACK) / PAGE_SIZE;
    swap_out(kva);
    free_mem_map[num] = asid;
    clean_page((uint64_t*)kva);
    return kva;
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

//va is the base of a page
swap_info_t* page_in_swap(reg_t va,reg_t asid)
{
    va = get_page_base(va);
    for(int j=0;j<SWAP_INFO_NUM;j++)
    {
        if((swap_info[j].va==va) && is_same_proc(swap_info[j].asid,asid))
        {
            return &swap_info[j];
        }
    }
    return 0;
}

//notice :choose a USER page to swap out
//return kva of the page
ptr_t choose_swap()
{
    while (1)
    {
        if(!(free_mem_map[clock_ptr]&KERNEL_PAGE) )//not kernel page 
        {
            if(!(free_mem_map[clock_ptr]&MAP_PAGE_ACCESSED))//not accessed page
            {
                int tmp = clock_ptr;
                clock_ptr = (clock_ptr+1)%MEM_PAGE_NUM;
                return tmp * PAGE_SIZE + INIT_KERNEL_STACK;
            }else
            {
                free_mem_map[clock_ptr] &= ~MAP_PAGE_ACCESSED;
            }
        }
        clock_ptr = (clock_ptr+1)%MEM_PAGE_NUM;
    }
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
        if((free_mem_map[i]&asid) ==asid) 
        {
            free_mem_map[i] = 0;
        }
    }
    for(int i=0;i<PGT_PAGE_NUM;i++)
    {
        if((free_pgt_map[i]& asid)==asid)
        {
            free_pgt_map[i] = 0;
        }
    }

    //free swap info that belong to the given asid
    for(int i=0;i<SWAP_INFO_NUM;i++)
    {
        if((swap_info[i].asid&asid)==asid)
        {
            swap_info[i].va = 0;
            swap_info[i].asid = 0;
            swap_info[i].sec_num = 0;
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
    set_attribute(&pgd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC  | _PAGE_USER);
    va_info[(kva - INIT_KERNEL_STACK)/PAGE_SIZE]= get_page_base(va);
    return kva;
}

//using pgdir in pa state is ok
PTE* get_pte(reg_t va,reg_t pgdir)
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
    if(pgd2[vpn0]==0)
        return 0;
    return &pgd2[vpn0];
}

void set_map_page_accessed(int index)
{
    free_mem_map[index] |= MAP_PAGE_ACCESSED;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}