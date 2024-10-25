#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la t0,__bss_start
	la t1,__BSS_END__
loop1:
	sw zero,(t0)
	addi t0,t0,4
	blt t0,t1,loop1

    /*save old return address*/
    addi sp,sp,-8
    sd ra,(sp)
    

    /* TODO: [p1-task3] enter main function */
    jal main
    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    ld ra,(sp)
	addi sp,sp,8
    ret


    /************************************************************/
    /* TODO: [P3] call sys_exit */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
