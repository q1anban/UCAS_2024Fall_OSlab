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
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/smp.h>

extern void ret_from_exception();

#define VERSION_BUF 50

#define TASK_NUM_LOC 0x502001f6
#define TASK_INFO_OFFSET 0x502001f8
#define KERNEL_LOC 0x50201000
#define MYSTERIOUS_PLACE 0x52000000

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];
int tasknum;

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
    sd_read(MYSTERIOUS_PLACE, num_sec, start_sec);
    char *ptr = (char *)(MYSTERIOUS_PLACE + offset % SECTOR_SIZE);
    for (int i = 0; i < num; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            tasks[i].name[j] = *ptr;
            ptr++;
        }
        tasks[i].offset = *((int *)ptr);
        ptr += 4;
        tasks[i].size = *((int *)ptr);
        ptr += 4;
    }
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
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
    pt_regs->regs[2] = user_stack;
    pt_regs->regs[4] = (reg_t)pcb;
    pt_regs->sepc = entry_point;
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
    pcb->user_sp = user_stack;
    pcb->kernel_sp = (ptr_t)pt_regs;
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    // initialize pid0 and other pcb
    pid0_pcb[0].cursor_x = 0;
    pid0_pcb[0].cursor_y = 0;
    pid0_pcb[0].pid = -1;//-1 means kernel process
    pid0_pcb[0].list.next = &pid0_pcb[0].list;
    pid0_pcb[0].list.prev = &pid0_pcb[0].list;
    pid0_pcb[0].status = TASK_RUNNING;
    pid0_pcb[0].wakeup_time = 0;
    pid0_pcb[0].wait_list.next = &pid0_pcb[0].wait_list;
    pid0_pcb[0].wait_list.prev = &pid0_pcb[0].wait_list;
    pid0_pcb[0].mbox_rw = -1;
    pid0_pcb[0].mutex_idx = -1;
    pid0_pcb[0].kernel_sp = allocKernelPage(1);

    pid0_pcb[1].cursor_x = 0;
    pid0_pcb[1].cursor_y = 0;
    pid0_pcb[1].pid = -1;// -1 mearns kernel process
    pid0_pcb[1].list.next = &pid0_pcb[1].list;
    pid0_pcb[1].list.prev = &pid0_pcb[1].list;
    pid0_pcb[1].status = TASK_RUNNING;
    pid0_pcb[1].wakeup_time = 0;
    pid0_pcb[1].wait_list.next = &pid0_pcb[1].wait_list;
    pid0_pcb[1].wait_list.prev = &pid0_pcb[1].wait_list;
    pid0_pcb[1].mbox_rw = -1;
    pid0_pcb[1].mutex_idx = -1;
    pid0_pcb[1].kernel_sp  =allocKernelPage(1);

    // initialize other pcb
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        pcb[i].pid = i;
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
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb[0];//only CPU 0 will excute this function
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_EXEC] = do_exec;
    syscall[SYSCALL_EXIT] = do_exit;
    syscall[SYSCALL_SLEEP] = do_sleep;
    syscall[SYSCALL_KILL] = do_kill;
    syscall[SYSCALL_WAITPID] = do_waitpid;
    syscall[SYSCALL_PS] = do_process_show;
    syscall[SYSCALL_GETPID] = do_getpid;
    syscall[SYSCALL_YIELD] = do_scheduler;
    syscall[SYSCALL_WRITE] = screen_write; //?
    syscall[SYSCALL_READCH] = port_read_ch;
    syscall[SYSCALL_CURSOR] = screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = screen_reflush;
    syscall[SYSCALL_CLEAR] = screen_clear;
    syscall[SYSCALL_GET_TIMEBASE] = get_time_base;
    syscall[SYSCALL_GET_TICK] = get_ticks;
    syscall[SYSCALL_LOCK_INIT] = do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = do_mutex_lock_release;
    syscall[SYSCALL_BARR_INIT] = do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = do_barrier_destroy;
    syscall[SYSCALL_COND_INIT] = do_condition_init;
    syscall[SYSCALL_COND_WAIT] = do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = do_condition_broadcast;
    syscall[SYSCALL_MBOX_OPEN] = do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = do_mbox_recv;
}
/************************************************************/

void test()
{
    char *str1 = "shell";
    reg_t entry;
    for (int i = 0; i < tasknum; i++)
    {
        if (strcmp(str1, tasks[i].name) == 0)
        {
            entry = load_task_img(i);
            init_pcb_stack(allocKernelPage(1), allocUserPage(1), entry, &pcb[0]);
            pcb[0].status = TASK_READY;
            LIST_ADD_TAIL(&ready_queue, &pcb[0].list);
        }
    }
}

int main(void)
{
    if (get_current_cpu_id() == 0) //CPU 0 entry here
    {        
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        printk("> [INIT] CPU frequency is %d Hz.\n", time_base);

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

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
