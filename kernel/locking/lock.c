#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <os/string.h>




mutex_lock_t mlocks[LOCK_NUM];

barrier_t barriers[BARRIER_NUM];

condition_t conditions[CONDITION_NUM];

semaphore_t semaphores[SEMAPHORE_NUM];

mailbox_t mboxes[MBOX_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i = 0; i < LOCK_NUM; i++)
    {
        mlocks[i].block_queue.next = &mlocks[i].block_queue;
        mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].key= -1;//-1 means no process has the lock
        mlocks[i].lock.status=UNLOCKED;
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
        mlocks[mlock_idx].owner = current_running->pid;
    }
    else//the lock is locked
    {
        //add current thread to the blocked queue
        list_node_t *node =&current_running->list;
        current_running->status = TASK_BLOCKED;
        do_block(node,&mlocks[mlock_idx].block_queue);
        do_scheduler();
    }
}

//special version of do_mutex_lock_acquire for kernel
//note:only be used in kernel mode since it does not do scheduler even if there are blocked processes
void do_mutex_lock_acquire_non_sched(pcb_t *pcb_,int mlock_idx)
{
    if (mlocks[mlock_idx].lock.status == UNLOCKED)//the lock is unlocked
    {
        mlocks[mlock_idx].lock.status = LOCKED;//aqcuire the lock
        mlocks[mlock_idx].owner = pcb_->pid;
    }
    else//the lock is locked
    {
        //add current thread to the blocked queue
        list_node_t *node = & pcb_->list;
        pcb_->status = TASK_BLOCKED;
        do_block(node,&mlocks[mlock_idx].block_queue);
    }
}

//we assume that the blocked process will not be woken up until the lock is released , so this function is for the running process
void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    if(current_running->pid == mlocks[mlock_idx].owner) //only the owner can release the lock
    {
        if(!LIST_IS_EMPTY(&mlocks[mlock_idx].block_queue))//only when there are blocked processes
        {
            //remove the first node from the blocked queue
            list_node_t *node = mlocks[mlock_idx].block_queue.next;
            do_unblock(node);
            pcb_t *pcb_ = NODE_TO_PCB(node);
            mlocks[mlock_idx].owner = pcb_->pid;
            pcb_->status = TASK_READY;
        }
        else//no more blocked processes
        {
            mlocks[mlock_idx].lock.status = UNLOCKED;
        }
    }
}

//release all locks owned by a process
void do_release_locks(int pid)
{
    for(int i = 0; i < LOCK_NUM; i++)
    {
        if(mlocks[i].lock.status == LOCKED &&mlocks[i].owner == pid)
        {
            if(!LIST_IS_EMPTY(&mlocks[i].block_queue))//only when there are blocked processes
            {
                //remove the first node from the blocked queue
                list_node_t *node = mlocks[i].block_queue.next;
                do_unblock(node);
                pcb_t *pcb_ = NODE_TO_PCB(node);
                mlocks[i].owner = pcb_->pid;
                pcb_->status = TASK_READY;
            }
            else//no more blocked processes
            {
                mlocks[i].lock.status = UNLOCKED;
            }
        }
    }
}

void init_barriers(void)
{
    for(int i = 0; i < BARRIER_NUM; i++)
    {
        barriers[i].key = -1;
        barriers[i].count =0;
        barriers[i].wait_queue.next = &barriers[i].wait_queue;
        barriers[i].wait_queue.prev = &barriers[i].wait_queue;
        barriers[i].goal = -1;
    }
}

int do_barrier_init(int key, int goal)
{
    //find the barrier specified by key
    for(int i = 0; i < BARRIER_NUM; i++)
    {
        if (barriers[i].key == key)
        {
            return i;
        }
    }
    //failed to find , try creating a new one and initialize it
    for(int i = 0; i < BARRIER_NUM; i++)
    {
        if (barriers[i].key == -1)
        {
            barriers[i].key = key;
            barriers[i].goal = goal;
            return i;
        }
    }
    return -1;//both failed
}

void do_barrier_wait(int barr_idx)
{
    //add current thread to the wait queue
    barriers[barr_idx].count++;
    if(barriers[barr_idx].count >= barriers[barr_idx].goal)
    {
        //all processes have reached the barrier
        barriers[barr_idx].count = 0;
        //remove all processes from the wait queue
        
        while(!LIST_IS_EMPTY(&barriers[barr_idx].wait_queue))
        {
            list_node_t *node = barriers[barr_idx].wait_queue.next;
            do_unblock(node);
            pcb_t *pcb_ = NODE_TO_PCB(node);
            pcb_->status = TASK_READY;
        }
    }else
    {
        //add current thread to the wait queue
        list_node_t *node = &current_running->list;
        current_running->status = TASK_BLOCKED;
        do_block(node,&barriers[barr_idx].wait_queue);
        do_scheduler();
    }
}

void do_barrier_destroy(int bar_idx)
{
    barriers[bar_idx].key = -1;
    barriers[bar_idx].count = 0;
    barriers[bar_idx].goal = -1;
    barriers[bar_idx].wait_queue.next = &barriers[bar_idx].wait_queue;
    barriers[bar_idx].wait_queue.prev = &barriers[bar_idx].wait_queue;

}

void init_conditions(void)
{
    for(int i = 0; i < CONDITION_NUM; i++)
    {
        conditions[i].key = -1;
        conditions[i].wait_queue.next = &conditions[i].wait_queue;
        conditions[i].wait_queue.prev = &conditions[i].wait_queue;
    }
}

int do_condition_init(int key)
{
    //find the condition specified by key
    for(int i = 0; i < CONDITION_NUM; i++)
    {
        if (conditions[i].key == key)
        {
            return i;
        }
    }
    //failed to find , try creating a new one and initialize it
    for(int i = 0; i < CONDITION_NUM; i++)
    {
        if (conditions[i].key == -1)
        {
            conditions[i].key = key;
            return i;
        }
    }
    return -1;//both failed
}

void do_condition_wait(int cond_idx, int mutex_idx)
{
    //add current thread to the wait queue
    do_mutex_lock_release(mutex_idx);
    list_node_t *node =& current_running->list;
    current_running->status = TASK_BLOCKED;
    current_running->mutex_idx = mutex_idx;
    do_block(node,&conditions[cond_idx].wait_queue);
    do_scheduler();
}

void do_condition_signal(int cond_idx)
{
    //remove the first node from the wait queue
    list_node_t *node = conditions[cond_idx].wait_queue.next;
    if(node != &conditions[cond_idx].wait_queue)
    {
        do_unblock(node);
        pcb_t *pcb_ = NODE_TO_PCB(node);
        pcb_->status = TASK_READY;
        //re-acquire the lock
        do_mutex_lock_acquire_non_sched(pcb_,pcb_->mutex_idx);
    }
}

void do_condition_broadcast(int cond_idx)
{
    //remove all nodes from the wait queue
    list_node_t *node = conditions[cond_idx].wait_queue.next;
    while (node != &conditions[cond_idx].wait_queue)
    {
        do_unblock(node);
        pcb_t *pcb_ = NODE_TO_PCB(node);
        pcb_->status = TASK_READY;
        //re-acquire the lock
        do_mutex_lock_acquire_non_sched(pcb_,pcb_->mutex_idx);
        node = conditions[cond_idx].wait_queue.next;
    }
}

void do_condition_destroy(int cond_idx)
{
    conditions[cond_idx].key = -1;
    conditions[cond_idx].wait_queue.next = &conditions[cond_idx].wait_queue;
    conditions[cond_idx].wait_queue.prev = &conditions[cond_idx].wait_queue;
}

void init_mbox(void)
{
    for(int i = 0; i < MBOX_NUM; i++)
    {
        mboxes[i].length=0;
        mboxes[i].name[0]='\0';
        mboxes[i].wait_queue.next = &mboxes[i].wait_queue;
        mboxes[i].wait_queue.prev = &mboxes[i].wait_queue;
        mboxes[i].ref_count=0;
    }
}

int do_mbox_open(char *name)
{
    //find the mbox specified by name
    for(int i = 0; i < MBOX_NUM; i++)
    {
        if (strcmp(mboxes[i].name,name) == 0)
        {
            mboxes[i].ref_count++;
            return i;
        }
    }
    //failed to find , try creating a new one and initialize it
    for(int i = 0; i < MBOX_NUM; i++)
    {
        if (mboxes[i].name[0] == '\0')
        {
            strcpy(mboxes[i].name,name);
            mboxes[i].ref_count++;
            return i;
        }
    }
    return -1;//both failed
}

void do_mbox_close(int mbox_idx)
{
    mboxes[mbox_idx].ref_count--;
    if(mboxes[mbox_idx].ref_count == 0)
    {
        mboxes[mbox_idx].length=0;
        mboxes[mbox_idx].name[0]='\0';
        mboxes[mbox_idx].wait_queue.next = &mboxes[mbox_idx].wait_queue;
        mboxes[mbox_idx].wait_queue.prev = &mboxes[mbox_idx].wait_queue;
    }
}

int do_mbox_send(int mbox_idx, void *msg,int msg_length)
{
    if(mboxes[mbox_idx].length + msg_length <=MAX_MBOX_LENGTH )//available space
    {
        memcpy(((uint8_t*)mboxes[mbox_idx].buffer+mboxes[mbox_idx].length),(uint8_t*)msg,msg_length);//finish sending
        mboxes[mbox_idx].length+=msg_length;
        
        list_node_t *node = mboxes[mbox_idx].wait_queue.next;
        list_node_t *temp = NULL;
        pcb_t *pcb_ = NULL;
        while (node != &mboxes[mbox_idx].wait_queue)
        {
            temp= node->next;
            pcb_ = NODE_TO_PCB(node);
            if(pcb_->mbox_rw==0 && mboxes[mbox_idx].length >= pcb_->mbox_size)//is read and exists enough data
            {
                memcpy((uint8_t*)(pcb_->mbox_buf),(uint8_t*)(mboxes[mbox_idx].buffer),pcb_->mbox_size);
                mboxes[mbox_idx].length-=pcb_->mbox_size;
                memcpy((uint8_t*)mboxes[mbox_idx].buffer,((uint8_t*)mboxes[mbox_idx].buffer+pcb_->mbox_size),mboxes[mbox_idx].length);
                pcb_->mbox_rw=-1;
                do_unblock(node);
                pcb_->status = TASK_READY;
                //TODO :set return value ... ... how ?
            }
            node = temp;
        }
        return msg_length;
    }else//no available space ,add current thread to the wait queue
    {
        current_running->status = TASK_BLOCKED;
        do_block(&current_running->list,&mboxes[mbox_idx].wait_queue);
        current_running->mbox_buf = msg;
        current_running->mbox_size = msg_length;
        current_running->mbox_rw = 1;

        regs_context_t *pt_regs = (regs_context_t *)current_running->kernel_sp;
        pt_regs->regs[10] = msg_length;
        do_scheduler();
        return msg_length;
    }
}

int do_mbox_recv(int mbox_idx, void *msg,int msg_length)
{
    if(mboxes[mbox_idx].length >= msg_length)//available data
    {
        memcpy((uint8_t*)msg,(uint8_t*)mboxes[mbox_idx].buffer,msg_length);//finish receiving
        mboxes[mbox_idx].length-=msg_length;
        memcpy((uint8_t*)mboxes[mbox_idx].buffer,((uint8_t*)mboxes[mbox_idx].buffer+msg_length),mboxes[mbox_idx].length);//finish reading

        list_node_t *node = mboxes[mbox_idx].wait_queue.next;
        list_node_t *temp = NULL;
        pcb_t *pcb_ = NULL;
        while (node != &mboxes[mbox_idx].wait_queue)
        {
            temp= node->next;
            pcb_ = NODE_TO_PCB(node);
            if(pcb_->mbox_rw==1 && mboxes[mbox_idx].length + pcb_->mbox_size <= MAX_MBOX_LENGTH)//is write and exists enough data
            {
                memcpy(((uint8_t*)mboxes[mbox_idx].buffer+mboxes[mbox_idx].length),(uint8_t*)pcb_->mbox_buf,pcb_->mbox_size);
                mboxes[mbox_idx].length+=pcb_->mbox_size;
                pcb_->mbox_rw=-1;
                do_unblock(node);
                pcb_->status = TASK_READY;
            }
            node = temp;
        }
        return msg_length;
    }else//no available data ,add current thread to the wait queue
    {
        current_running->status = TASK_BLOCKED;
        do_block(&current_running->list,&mboxes[mbox_idx].wait_queue);
        current_running->mbox_buf = msg;
        current_running->mbox_size = msg_length;
        current_running->mbox_rw = 0;

        regs_context_t *pt_regs = (regs_context_t *)current_running->kernel_sp;
        pt_regs->regs[10] = msg_length;
        do_scheduler();
        return msg_length;
    }
}


    