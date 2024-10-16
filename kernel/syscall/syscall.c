#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    //have to be the first to handle the syscall exception,cause we have modified the ra
    regs->sepc= regs->sepc+4;


    int64_t fn = regs->regs[17];
    int64_t arg0 = regs->regs[10];
    int64_t arg1 = regs->regs[11];
    int64_t arg2 = regs->regs[12];
    int64_t arg3 = regs->regs[13];
    int64_t arg4 = regs->regs[14];
    regs->regs[10] = syscall[fn](arg0, arg1, arg2,arg3,arg4);
}
