#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    pcb_t *next_pcb = current_running;//if no process is ready , next_pcb will be current_running
    list_node_t *node;
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    if(!LIST_IS_EMPTY(&ready_queue))
    {
        //delete the first node of ready_queue
        node = ready_queue.next;
        next_pcb = NODE_TO_PCB(node);
        next_pcb->status = TASK_RUNNING;
        LIST_REMOVE(node);
        NODE_REFRESH(node);

        //add the current_running to the end of ready_queue
        if(current_running->status == TASK_RUNNING)
        {   
            LIST_ADD_TAIL(&ready_queue, &current_running->list);
            current_running->status = TASK_READY;
        }
    }

    current_running = next_pcb;
    // TODO: [p2-task1] switch_to current_running
    //switch_to(current_running, next_pcb);
    ret_from_exception();
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    current_running->status = TASK_BLOCKED;
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
}

//remove pcb_node from the ready_queue and add it to the block_queue. after that, call do_scheduler()
void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    LIST_REMOVE(pcb_node);
    LIST_ADD_TAIL(queue, pcb_node);
    do_scheduler();
    ret_from_exception();
}

//simply remove pcb_node from block_queue and add it to the ready_queue
void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    LIST_REMOVE(pcb_node);
    LIST_ADD_HEAD(&ready_queue, pcb_node);
}
