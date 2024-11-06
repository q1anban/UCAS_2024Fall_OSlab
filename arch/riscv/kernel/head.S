/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

#define SECOND_KERNEL_STACK	0x504F0000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* Check if we are the primary hart */
  csrr t0,CSR_MHARTID
  bnez t0,secondary

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la t0,__bss_start
	la t1,__BSS_END__
loop1:
	sb zero,(t0)
	addi t0,t0,1
	blt t0,t1,loop1

  /* TODO: [p1-task2] setup C environment */
  lui sp,%hi(KERNEL_STACK)
	addi sp,sp,%lo(KERNEL_STACK)
	j main

  secondary:

  lui sp,%hi(SECOND_KERNEL_STACK)
  addi sp,sp,%lo(SECOND_KERNEL_STACK)
	j main


loop:
  wfi
  j loop

END(_start)
