#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#include <os/loader.h>
#include <os/task.h>
#include <csr.h>
#include <os/string.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb[2];

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;
int get_child_thread_num(int pid)
{
    int num = 0;
    for(int i = 0; i < NUM_MAX_TASK; i++)
    {
        if(pcb[i].parent_pid == pid)
            num++;
    }   
    return num;
}

void do_scheduler(void)
{
    pcb_t *next_pcb = current_running; // if no process is ready , next_pcb will be current_running
    list_node_t *node;
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
    check_sleeping();
    // TODO: [p2-task1] Modify the current_running pointer.
    if (!LIST_IS_EMPTY(&ready_queue))
    {
        // delete the first node of ready_queue
        node = ready_queue.next;
        next_pcb = NODE_TO_PCB(node);
        next_pcb->status = TASK_RUNNING;
        LIST_REMOVE(node);
        NODE_REFRESH(node);

        // add the current_running to the end of ready_queue
        if (current_running->status == TASK_RUNNING)
        {
            LIST_ADD_TAIL(&ready_queue, &current_running->list);
            current_running->status = TASK_READY;
        }
    }
    if (next_pcb->asid != current_running->asid)
    {
        set_satp(SATP_MODE_SV39, next_pcb->asid, next_pcb->satp >> NORMAL_PAGE_SHIFT);
        local_flush_tlb_all();
        if (current_running->status == TASK_EXITED)
            freeProcessMem(current_running->asid);
    }

    current_running = next_pcb;
    // TODO: [p2-task1] switch_to current_running
    // switch_to(current_running, next_pcb);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    current_running->status = TASK_BLOCKED;
    LIST_REMOVE(&current_running->list);
    LIST_ADD_TAIL(&sleep_queue, &current_running->list);
    // 2. set the wake up time for the blocked task
    current_running->wakeup_time = get_ticks() + sleep_time * time_base;
    // 3. reschedule because the current_running is blocked.
    do_scheduler();
}

// remove pcb_node from the ready_queue and add it to the block_queue. after that, call do_scheduler()
void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    LIST_REMOVE(pcb_node);
    LIST_ADD_TAIL(queue, pcb_node);
}

// simply remove pcb_node from block_queue and add it to the ready_queue
void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    LIST_REMOVE(pcb_node);
    LIST_ADD_HEAD(&ready_queue, pcb_node);
}

pid_t do_exec(char *name, int argc, char *argv[])
{
    for (int i = 0; i < TASK_MAXNUM; i++)
    {
        if (!strcmp(tasks[i].name, name)) // find the task
        {
            for (int j = 0; j < NUM_MAX_TASK; j++)
            {
                if (pcb[j].status == TASK_EXITED) // find an empty pcb
                {
                    // initialize the pcb
                    // init page table
                    pcb[j].asid = ASID_USER | j;
                    pcb[j].satp = load_task_img(i, pcb[j].asid); // page table
                    pcb[j].parent_pid = -1;
                    // map USER_STACK_ADDR-PAGE_SIZE ~ USER_STACK_ADDR to user stack
                    reg_t real_user_sp = alloc_page_helper(USER_STACK_ADDR - PAGE_SIZE, pa2kva(pcb[j].satp), pcb[j].asid);
                    //"user_stack" in kernel
                    // user_stack    ------ >  USER_STACK_ADDR
                    //
                    //
                    //
                    // real_user_sp  ----- >   USER_STACK_ADDR - PAGE_SIZE
                    reg_t user_stack = real_user_sp + PAGE_SIZE;
                    reg_t kva2uva = user_stack - USER_STACK_ADDR; // to turn kva to uva need to add - kva2uva
                    reg_t kernel_stack = allocPage(pcb[j].asid | KERNEL_PAGE) + PAGE_SIZE;
                    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
                    for (int i = 0; i < 32; i++)
                    {
                        pt_regs->regs[i] = 0;
                    }

                    reg_t space = 0; // for arguments
                    reg_t pointers = user_stack - argc * sizeof(char *);
                    pt_regs->regs[11] = pointers - kva2uva;
                    // notice: now we have to transfer all the kva to uva
                    // prepare space for arguments
                    for (int k = 0; k < argc; k++)
                    {
                        space += strlen(argv[k]) + 1 + sizeof(char *); // size of pointer + '\0' + string length
                    }
                    user_stack = user_stack - space; // allocate space for arguments
                    reg_t arg_pointer = user_stack;
                    for (int k = 0; k < argc; k++)
                    {
                        strcpy((char *)(arg_pointer), argv[k]);
                        *((char **)(pointers)) = (char *)(arg_pointer - kva2uva);

                        arg_pointer += strlen(argv[k]) + 1; // point to space for the next string
                        pointers += sizeof(char *);
                    }

                    user_stack = ROUNDDOWN(user_stack, 128); // align
                    pt_regs->regs[2] = USER_STACK_ADDR + user_stack - real_user_sp - PAGE_SIZE;
                    pt_regs->regs[4] = (reg_t)&pcb[j];
                    pt_regs->regs[10] = argc;

                    pt_regs->sepc = 0x10000;
                    pt_regs->scause = 0x0;
                    pt_regs->sstatus = SR_SPIE;  // indicates that before interrupts are enabled
                    pt_regs->sstatus &= ~SR_SPP; // indicates user mode
                    pt_regs->sbadaddr = 0x0;
                    pcb[j].status = TASK_READY;
                    LIST_ADD_TAIL(&ready_queue, &pcb[j].list);
                    pcb[j].pid = j;
                    LIST_REMOVE(&pcb[j].wait_list);
                    pcb[j].kernel_sp = pt_regs;
                    pcb[j].user_sp = user_stack;

                    return j;
                }
            }
        }
    }
    return -1;
}

void do_exit(void)
{
    // kill itself is exit
    do_kill_all(current_running->pid);
    do_scheduler();
}

int do_kill_all(pid_t pid)
{
    for(int i = 0; i < NUM_MAX_TASK; i++)
    {
        if(pcb[i].parent_pid == pid && pcb[i].status != TASK_EXITED)
        {
            do_kill(i);
        }
    }
    do_kill(pid);
    freeProcessMem(pcb[pid].asid);
}

int do_kill(pid_t pid)
{
    // remove it from the ready_queue or sleep_queue
    LIST_REMOVE(&pcb[pid].list);
    // release its lock

    do_release_locks(pid);
    // wake its waiters
    while (!LIST_IS_EMPTY(&pcb[pid].wait_list))
    {
        list_node_t *node = pcb[pid].wait_list.next;
        pcb_t *waiter = NODE_TO_PCB(node);
        waiter->status = TASK_READY;
        do_unblock(node);
    }
    // declare the process as exited
    pcb[pid].status = TASK_EXITED;
}

int do_waitpid(pid_t pid)
{
    if (pcb[pid].status != TASK_EXITED)
    {
        do_block(&current_running->list, &pcb[pid].wait_list);
        current_running->status = TASK_BLOCKED;
        do_scheduler();
    }
}

void do_process_show()
{
    screen_write("[Process table]\n");
    int num = 0;
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].status != TASK_EXITED)
        {
            screen_write_ch('[');
            screen_write_ch(num + '0');

            screen_write("]:PID ");
            screen_write_ch(pcb[i].pid + '0');
            screen_write(" , STATUS ");
            if (pcb[i].status == TASK_RUNNING)
            {
                screen_write("RUNNING\n");
            }
            else if (pcb[i].status == TASK_READY)
            {
                screen_write("READY\n");
            }
            else if (pcb[i].status == TASK_BLOCKED)
            {
                screen_write("BLOCKED\n");
            }
            num++;
        }
    }
}

pid_t do_getpid()
{
    return current_running->pid;
}

void do_thread_create(pid_t *thread, void (*start_routine)(void *), void *arg)
{
    for (int j = 0; j < NUM_MAX_TASK; j++)
    {
        if (pcb[j].status == TASK_EXITED) // find an empty pcb
        {
            // initialize the pcb
            // init page table
            pcb[j].asid = current_running->asid;
            pcb[j].satp = current_running->satp; // same page table
            pcb[j].parent_pid = current_running->pid;
            int num = get_child_thread_num(current_running->pid);
            num++;
            // map USER_STACK_ADDR-PAGE_SIZE ~ USER_STACK_ADDR to user stack
            reg_t real_user_sp = alloc_page_helper(USER_STACK_ADDR - num*PAGE_SIZE, pa2kva(pcb[j].satp), pcb[j].asid);
            reg_t kernel_stack = allocPage(pcb[j].asid | KERNEL_PAGE) + PAGE_SIZE;
            regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
            for (int i = 0; i < 32; i++)
            {
                pt_regs->regs[i] = 0;
            }
            // notice: now we have to transfer all the kva to uva
            // prepare space for arguments

            pt_regs->regs[2] = USER_STACK_ADDR - num*PAGE_SIZE;
            pt_regs->regs[4] = (reg_t)&pcb[j];
            pt_regs->regs[10] = arg;

            pt_regs->sepc = start_routine;
            pt_regs->scause = 0x0;
            pt_regs->sstatus = SR_SPIE;  // indicates that before interrupts are enabled
            pt_regs->sstatus &= ~SR_SPP; // indicates user mode
            pt_regs->sbadaddr = 0x0;
            pcb[j].status = TASK_READY;
            LIST_ADD_TAIL(&ready_queue, &pcb[j].list);
            pcb[j].pid = j;
            LIST_REMOVE(&pcb[j].wait_list);
            pcb[j].kernel_sp = pt_regs;
            *thread = j;
            return;
        }
    }
}
int do_thread_join(pid_t thread)
{
    return do_waitpid(thread);
}
