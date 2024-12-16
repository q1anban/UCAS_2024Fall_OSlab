/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
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

#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/ioremap.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/smp.h>
#include <os/net.h>

extern void ret_from_exception();

#define VERSION_BUF 50

#define TASK_NUM_LOC 0x502001f6lu
#define TASK_INFO_OFFSET 0x502001f8lu
#define KERNEL_LOC 0x50201000lu
#define MYSTERIOUS_PLACE 0x52000000lu

#define USER_STACK 0xf00010000

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];
int tasknum;

int padding_start_sec;

/*
 * Once a CPU core calls this function,
 * it will stop executing!
 */
// static void kernel_brake(void)
// {
//     disable_interrupt();
//     while (1)
//         __asm__ volatile("wfi");
// }

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR] = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ] = (long (*)())sd_read;
    jmptab[SD_WRITE] = (long (*)())sd_write;
    jmptab[QEMU_LOGGING] = (long (*)())qemu_logging;
    jmptab[SET_TIMER] = (long (*)())set_timer;
    jmptab[READ_FDT] = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR] = (long (*)())screen_move_cursor;
    jmptab[PRINT] = (long (*)())printk;
    jmptab[YIELD] = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT] = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ] = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE] = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.
    jmptab[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    // use the exact position to find task info
    int16_t *ptasknum = (int16_t *)TASK_NUM_LOC;
    int32_t *taskinfo_offset = (int32_t *)TASK_INFO_OFFSET;

    int num = *ptasknum;
    tasknum = num;
    int offset = *taskinfo_offset;
    int infosize = sizeof(task_info_t) * num;
    int start_sec = offset / SECTOR_SIZE;
    int end_sec = (offset + infosize) / SECTOR_SIZE;
    int num_sec = end_sec - start_sec + 1;
    sd_read_more(MYSTERIOUS_PLACE, num_sec, start_sec);
    char *ptr = (char *)pa2kva(MYSTERIOUS_PLACE + offset % SECTOR_SIZE);
    for (int i = 0; i < num; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            tasks[i].name[j] = *ptr;
            ptr++;
        }
        tasks[i].offset = *((int *)ptr);
        ptr += 4;
        tasks[i].file_size = *((int *)ptr);
        ptr += 4;
        tasks[i].mem_size = *((int *)ptr);
        ptr += 4;
    }
    int tmp=0;
    for (int i = 0; i < num; i++)
    {
        if(tmp<tasks[i].offset+tasks[i].file_size)
        {
            tmp=tasks[i].offset+tasks[i].file_size;
        }
    }
    padding_start_sec = ROUND(tmp/SECTOR_SIZE,8);
}

/************************************************************/
//kernel stack  and user stack are both kva
static void init_pcb_stack(
    ptr_t kernel_stack,pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
    {
        pt_regs->regs[i] = 0;
    }
    //notice : add mapping here for user stack
    pt_regs->regs[2] = USER_STACK;
    //alloc_page_helper(USER_STACK-PAGE_SIZE,pa2kva(pcb->satp),pcb->asid);
    pt_regs->regs[4] = (reg_t)pcb;
    pt_regs->sepc = 0x10000;
    pt_regs->scause = 0x0;
    pt_regs->sstatus = SR_SPIE;  // indicates that before interrupts are enabled
    pt_regs->sstatus &= ~SR_SPP; // indicates user mode
    pt_regs->sbadaddr = 0x0;

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    /*
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    for(int i=0;i<14;i++)
    {
        pt_switchto->regs[i]=0;
    }
    pt_switchto->regs[0] = entry_point;//ra
    pt_switchto->regs[1] = user_stack;//sp
    */
    pcb->user_sp = USER_STACK;
    pcb->kernel_sp = (ptr_t)pt_regs;
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    // initialize pid0 and other pcb
    pid0_pcb[0].asid = ASID_KERNEL|0;
    pid0_pcb[0].cursor_x = 0;
    pid0_pcb[0].cursor_y = 0;
    pid0_pcb[0].pid = -1;//-1 means kernel process
    pid0_pcb[0].parent_pid = -1;
    pid0_pcb[0].list.next = &pid0_pcb[0].list;
    pid0_pcb[0].list.prev = &pid0_pcb[0].list;
    pid0_pcb[0].status = TASK_RUNNING;
    pid0_pcb[0].wakeup_time = 0;
    pid0_pcb[0].wait_list.next = &pid0_pcb[0].wait_list;
    pid0_pcb[0].wait_list.prev = &pid0_pcb[0].wait_list;
    pid0_pcb[0].mbox_rw = -1;
    pid0_pcb[0].mutex_idx = -1;
    pid0_pcb[0].kernel_sp = allocPage(pid0_pcb[0].asid|KERNEL_PAGE)+PAGE_SIZE;//notice that sp is the top of the page
    pid0_pcb[0].satp = PGDIR_PA;

    pid0_pcb[1].asid = ASID_KERNEL|1;
    pid0_pcb[1].cursor_x = 0;
    pid0_pcb[1].cursor_y = 0;
    pid0_pcb[1].pid = -1;// -1 mearns kernel process
    pid0_pcb[1].parent_pid = -1;
    pid0_pcb[1].list.next = &pid0_pcb[1].list;
    pid0_pcb[1].list.prev = &pid0_pcb[1].list;
    pid0_pcb[1].status = TASK_RUNNING;
    pid0_pcb[1].wakeup_time = 0;
    pid0_pcb[1].wait_list.next = &pid0_pcb[1].wait_list;
    pid0_pcb[1].wait_list.prev = &pid0_pcb[1].wait_list;
    pid0_pcb[1].mbox_rw = -1;
    pid0_pcb[1].mutex_idx = -1;
    pid0_pcb[1].kernel_sp  =allocPage(pid0_pcb[1].asid|KERNEL_PAGE)+PAGE_SIZE;
    pid0_pcb[1].satp = PGDIR_PA;
    

    // initialize other pcb
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        pcb[i].pid = i;
        pcb[i].parent_pid = -1;
        pcb[i].status = TASK_EXITED;
        pcb[i].list.next = &pcb[i].list;
        pcb[i].list.prev = &pcb[i].list;
        pcb[i].wakeup_time = 0;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;
        pcb[i].wait_list.next = &pcb[i].wait_list;
        pcb[i].wait_list.prev = &pcb[i].wait_list;
        pcb[i].mbox_rw = -1;
        pcb[i].mutex_idx = -1;
        pcb[i].asid = ASID_USER|i;

        pcb[i].net_curr_num=0;
        pcb[i].net_length = 0;
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb[0];//only CPU 0 will excute this function
}

static void clean_temp_page(void)
{
    //clean up temporary page that maps 0x50000000-0x51000000 to themselves
    
    uint64_t *pgaddr = (uint64_t *)pa2kva(PGDIR_PA);
    uint64_t* second_pgaddr = (uint64_t*)pa2kva(get_pa(pgaddr[get_vpn2(0x50000000lu)]));//find the second page table
    for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu;
         pa += 0x200000lu) 
    {
        second_pgaddr[get_vpn1(pa)]=0;
    }
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_EXEC] = (long (*)())do_exec;
    syscall[SYSCALL_EXIT] = (long (*)())do_exit;
    syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
    syscall[SYSCALL_KILL] = (long (*)())do_kill_all;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_PS] = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
    syscall[SYSCALL_THREAD_CREATE] = (long (*)())do_thread_create;
    syscall[SYSCALL_THREAD_JOIN] = (long (*)())do_thread_join;
    syscall[SYSCALL_WRITE] =(long (*)()) screen_write; 
    syscall[SYSCALL_READCH] = (long (*)())port_read_ch;
    syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT] = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT] = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;
    syscall[SYSCALL_NET_SEND] = (long (*)())do_net_send;
    syscall[SYSCALL_NET_RECV] = (long (*)())do_net_recv;

}
/************************************************************/

void test()
{
    char *str1 = "shell";
    for (int i = 0; i < tasknum; i++)
    {
        if (strcmp(str1, tasks[i].name) == 0)
        {
            
            pcb[0].status = TASK_READY;
            pcb[0].satp = load_task_img(i,pcb[0].asid);
            init_pcb_stack(allocPage(pcb[0].asid|KERNEL_PAGE)+PAGE_SIZE,&pcb[0]);//kernel sp  using  kva
            LIST_ADD_TAIL(&ready_queue, &pcb[0].list);
        }
    }
}

int main(void)
{
    if (get_current_cpu_id() == 0) //CPU 0 entry here
    {        
        // Init memory
        init_mm();

        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read Flatten Device Tree (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        printk("> [INIT] CPU frequency is %d Hz.\n", time_base);

        e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // TODO: [p5-task4] Init plic
        plic_init(plic_addr, nr_irqs);
        printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        // Init barrier(●'◡'●)
        init_barriers();
        printk("> [INIT] Barrier initialization succeeded.\n");

        // Init condision variable (๑¯ω¯๑)
        init_conditions();
        printk("> [INIT] Condition variable initialization succeeded.\n");

        // Init mailbox ( ๑͒･(ｴ)･๑͒)
        init_mbox();
        printk("> [INIT] Mailbox initialization succeeded.\n");

        
        
        // Init Shell
        test();
        printk("> [INIT] Task initialization succeeded.\n");

        // Wake up CPU 1
        wakeup_other_hart();
        printk("> [INIT] Waking up CPU 1.\n");
        
        // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
        //   and then execute them.

        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's
        set_timer(get_ticks() + TIMER_INTERVAL);
        clean_temp_page();
        // do_scheduler();
        // ret_from_exception();
        // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
        while (1)
        {
            // If you do non-preemptive scheduling, it's used to surrender control

            // do_scheduler();
            // ret_from_exception();
            // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
            enable_preempt();
            asm volatile("wfi");
        }
    }
    else
    {
        // second cpu starts here
        current_running = &pid0_pcb[1];
        setup_exception();
        set_timer(get_ticks() + TIMER_INTERVAL);
        enable_interrupt();
        while(1){
            enable_preempt();
            asm volatile("wfi");
        }

    }

    
    return 0;
}
