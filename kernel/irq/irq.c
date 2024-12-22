#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <plic.h>
#include <os/net.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>

//#define PYNQ

#ifdef PYNQ
#define PLIC_E1000_IRQ 3
#else
#define PLIC_E1000_IRQ 33
#endif

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    //now ra is ret from exception
    uint64_t flag = 1lu<<63;
    uint32_t is_irq = scause >>63; 
    uint64_t cause = scause & ~flag;
    if(is_irq) //irq
    {
        irq_table[cause](regs, stval, scause);
    }else
    {
        //exception
        exc_table[cause](regs, stval, scause);
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    screen_reflush();
    do_scheduler();
}

void handle_load_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    PTE* pte = get_pte(stval,current_running->satp);
    if(pte!=0)//have been mapped, in memory or in swap
    {
        swap_info_t* info = page_in_swap(stval,current_running->asid);
        if(info!=0)//in swap
        {
            reg_t kva = allocPage(current_running->asid);
            swap_in(kva,info);
            //rebuild the mapping

            set_pfn(pte,kva2pa(kva)>>NORMAL_PAGE_SHIFT);
            set_attribute(pte,_PAGE_PRESENT|_PAGE_ACCESSED);
            int index = (kva-INIT_KERNEL_STACK)/PAGE_SIZE;
            set_map_page_accessed(index);
        }else//bad attribute
        {
            set_attribute(pte,_PAGE_ACCESSED|_PAGE_PRESENT);
            //set access bit for the page
            reg_t kva = pa2kva(get_pa(*pte));
            int index = (kva-INIT_KERNEL_STACK)/PAGE_SIZE;
            set_map_page_accessed(index);
        }
    }else
    {
        //not in memory and not in swap
        alloc_page_helper(stval,pa2kva(current_running->satp),current_running->asid);
    }
    
}

void handle_store_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{

    PTE* pte = get_pte(stval,current_running->satp);
    if(pte!=0)//have been mapped, in memory or in swap
    {
        swap_info_t* info = page_in_swap(stval,current_running->asid);
        if(info!=0)//in swap
        {
            reg_t kva = allocPage(current_running->asid);
            swap_in(kva,info);

            set_pfn(pte,kva2pa(kva)>>NORMAL_PAGE_SHIFT);//re build the mapping
            set_attribute(pte,_PAGE_ACCESSED|_PAGE_DIRTY|_PAGE_PRESENT);
            int index = (kva-INIT_KERNEL_STACK)/PAGE_SIZE;
            set_map_page_accessed(index);
        }else//bad attribute
        {
            set_attribute(pte,_PAGE_ACCESSED|_PAGE_PRESENT|_PAGE_DIRTY);
            //set access bit for the page
            reg_t kva = pa2kva(get_pa(*pte));
            int index = (kva-INIT_KERNEL_STACK)/PAGE_SIZE;
            set_map_page_accessed(index);
        }
    }else
    {
        //not in memory and not in swap
        alloc_page_helper(stval,pa2kva(current_running->satp),current_running->asid);
    }
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task4] external interrupt handler.
    printk("scause %lx\n", scause);
    // Note: plic_claim and plic_complete will be helpful ...
    uint32_t irq_ext = plic_claim();
    if(irq_ext == PLIC_E1000_IRQ)
    {
        net_handle_irq();
    }
    plic_complete(irq_ext);
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    exc_table[EXCC_SYSCALL] = handle_syscall;
    exc_table[EXCC_BREAKPOINT] = handle_other;
    exc_table[EXCC_INST_MISALIGNED] = handle_other;
    exc_table[EXCC_INST_ACCESS]=handle_other;
    exc_table[EXCC_LOAD_ACCESS]=handle_other;
    exc_table[EXCC_STORE_ACCESS]=handle_other;
    exc_table[EXCC_INST_PAGE_FAULT]=handle_load_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT]=handle_load_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT]=handle_store_page_fault;
    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    irq_table[IRQC_U_TIMER] = handle_other;
    irq_table[IRQC_S_TIMER] = handle_irq_timer;
    irq_table[IRQC_M_TIMER] = handle_other;
    irq_table[IRQC_U_EXT] = handle_other;
    irq_table[IRQC_S_EXT] = handle_irq_ext;
    irq_table[IRQC_M_EXT] = handle_other;
    irq_table[IRQC_U_SOFT] = handle_other;
    irq_table[IRQC_S_SOFT] = handle_other;//
    irq_table[IRQC_M_SOFT] = handle_other;
    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lx\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
