#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

//os precise size location
.equ os_precise_size_loc ,0x502001f8

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	addi a7,zero,BIOS_PUTSTR
	lui a0,%hi(msg)
	addi a0,a0,%lo(msg)
	jal  bios_func_entry

	// TODO: [p1-task2] call BIOS to read kernel in SD card
	li a7,BIOS_SDREAD
	li a0,kernel
	la t0,os_size_loc
	lh a1,(t0)
	li a2,1
	jal bios_func_entry

	// TODO: [p1-task4] load task-related arguments and pass them to kernel

	//lw a0,os_precise_size_loc
	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	jal kernel
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's chenweilun's bootloader...\n\r"

