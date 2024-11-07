#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    const unsigned long mask = 2;
    send_ipi(&mask); 
}

int locked=0;

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    while (atomic_swap(1,&locked))
        ;
    
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    locked=0;
}
