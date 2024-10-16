#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i = 0; i < LOCK_NUM; i++)
    {
        mlocks[i].block_queue.next = &mlocks[i].block_queue;
        mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].key= -1;//-1 means no process has the lock
        mlocks[i].lock.status=UNLOCKED;
        printk("lock %d initialized\n",i);
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */

    //find the lock specified by key
    for(int i = 0; i < LOCK_NUM; i++)
    {
        if (mlocks[i].key == key)
        {
            return i;
        }
    }
    //failed to find , try to create a new one
    for(int i = 0; i < LOCK_NUM; i++)
    {
        if (mlocks[i].key == -1)
        {
            mlocks[i].key = key;
            return i;
        }
    }
    return -1;//both failed
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    if (mlocks[mlock_idx].lock.status == UNLOCKED)//the lock is unlocked
    {
        mlocks[mlock_idx].lock.status = LOCKED;//aqcuire the lock
    }
    else//the lock is locked
    {
        //add current thread to the blocked queue
        list_node_t *node = PCB_TO_NODE(current_running);
        current_running->status = TASK_BLOCKED;
        do_block(&mlocks[mlock_idx].block_queue, node);
    }
    
}

//we assume that the blocked process will not be woken up until the lock is released , so this function is for the running process
void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    if(!LIST_IS_EMPTY(&mlocks[mlock_idx].block_queue))//only when there are blocked processes
    {
        //remove the first node from the blocked queue
        list_node_t *node = mlocks[mlock_idx].block_queue.next;
        do_unblock(node);
        pcb_t *pcb_ = NODE_TO_PCB(node);
        pcb_->status = TASK_READY;
    }
    if(LIST_IS_EMPTY(&mlocks[mlock_idx].block_queue))//no more blocked processes
    {
        mlocks[mlock_idx].lock.status = UNLOCKED;
    }
}
