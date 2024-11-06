#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */
  csrw CSR_SSCRATCH, sp //save sp temporarily
  ld sp,PCB_KERNEL_SP(tp) //kernel stack pointer

  sd ra,OFFSET_REG_RA(sp) //save ra
  csrr ra,CSR_SSCRATCH//real sp
  sd ra,OFFSET_REG_SP(sp) //save sp
  sd gp,OFFSET_REG_GP(sp) //save gp
  sd tp,OFFSET_REG_TP(sp) //save tp

  sd t0,OFFSET_REG_T0(sp) //save t0
  sd t1,OFFSET_REG_T1(sp) //save t1
  sd t2,OFFSET_REG_T2(sp) //save t2

  sd s0,OFFSET_REG_S0(sp) //save s0
  sd s1,OFFSET_REG_S1(sp) //save s1

  sd a0,OFFSET_REG_A0(sp) //save a0
  sd a1,OFFSET_REG_A1(sp) //save a1
  sd a2,OFFSET_REG_A2(sp) //save a2
  sd a3,OFFSET_REG_A3(sp) //save a3
  sd a4,OFFSET_REG_A4(sp) //save a4
  sd a5,OFFSET_REG_A5(sp) //save a5
  sd a6,OFFSET_REG_A6(sp) //save a6
  sd a7,OFFSET_REG_A7(sp) //save a7

  sd s2,OFFSET_REG_S2(sp) //save s2
  sd s3,OFFSET_REG_S3(sp) //save s3
  sd s4,OFFSET_REG_S4(sp) //save s4
  sd s5,OFFSET_REG_S5(sp) //save s5
  sd s6,OFFSET_REG_S6(sp) //save s6
  sd s7,OFFSET_REG_S7(sp) //save s7
  sd s8,OFFSET_REG_S8(sp) //save s8
  sd s9,OFFSET_REG_S9(sp) //save s9
  sd s10,OFFSET_REG_S10(sp) //save s10
  sd s11,OFFSET_REG_S11(sp) //save s11

  sd t3,OFFSET_REG_T3(sp) //save t3
  sd t4,OFFSET_REG_T4(sp) //save t4
  sd t5,OFFSET_REG_T5(sp) //save t5
  sd t6,OFFSET_REG_T6(sp) //save t6
  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS
  csrrc t1,CSR_SSTATUS, t0
  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */
  sd t1,OFFSET_REG_SSTATUS(sp) //save sstatus

  csrr t1,CSR_SEPC //save sepc
  sd t1,OFFSET_REG_SEPC(sp)

  csrr t1,CSR_STVAL //save stval
  sd t1,OFFSET_REG_SBADADDR(sp)

  csrr t1,CSR_SCAUSE //save scause
  sd t1,OFFSET_REG_SCAUSE(sp)

.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
  ld sp,PCB_KERNEL_SP(tp) //set sp to kernel stack
  ld ra,OFFSET_REG_RA(sp) //restore ra

  ld gp,OFFSET_REG_GP(sp) //restore gp
  ld tp,OFFSET_REG_TP(sp) //restore tp

  ld t0,OFFSET_REG_SSTATUS(sp) //restore sstatus
  csrw CSR_SSTATUS, t0 //restore sstatus

  ld t0,OFFSET_REG_SEPC(sp) //restore sepc
  csrw CSR_SEPC, t0 //restore sepc

  ld t0,OFFSET_REG_T0(sp) //restore t0
  ld t1,OFFSET_REG_T1(sp) //restore t1
  ld t2,OFFSET_REG_T2(sp) //restore t2

  ld s0,OFFSET_REG_S0(sp) //restore s0
  ld s1,OFFSET_REG_S1(sp) //restore s1

  ld a0,OFFSET_REG_A0(sp) //restore a0
  ld a1,OFFSET_REG_A1(sp) //restore a1
  ld a2,OFFSET_REG_A2(sp) //restore a2
  ld a3,OFFSET_REG_A3(sp) //restore a3
  ld a4,OFFSET_REG_A4(sp) //restore a4
  ld a5,OFFSET_REG_A5(sp) //restore a5
  ld a6,OFFSET_REG_A6(sp) //restore a6
  ld a7,OFFSET_REG_A7(sp) //restore a7

  ld s2,OFFSET_REG_S2(sp) //restore s2
  ld s3,OFFSET_REG_S3(sp) //restore s3
  ld s4,OFFSET_REG_S4(sp) //restore s4
  ld s5,OFFSET_REG_S5(sp) //restore s5
  ld s6,OFFSET_REG_S6(sp) //restore s6
  ld s7,OFFSET_REG_S7(sp) //restore s7
  ld s8,OFFSET_REG_S8(sp) //restore s8
  ld s9,OFFSET_REG_S9(sp) //restore s9
  ld s10,OFFSET_REG_S10(sp) //restore s10
  ld s11,OFFSET_REG_S11(sp) //restore s11

  ld t3,OFFSET_REG_T3(sp) //restore t3
  ld t4,OFFSET_REG_T4(sp) //restore t4
  ld t5,OFFSET_REG_T5(sp) //restore t5
  ld t6,OFFSET_REG_T6(sp) //restore t6

  ld sp,OFFSET_REG_SP(sp) //restore sp from kernel stack
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  /*addi sp, sp, -(SWITCH_TO_SIZE) */

  /*now current pcb is in a0, next pcb is in a1*/
  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  /*save current pcb , t0 point to kernel stack*/

  ld t0,  PCB_KERNEL_SP(tp)/*now t0 point to kernel stack*/
  sd ra,  SWITCH_TO_RA(t0)
  sd sp,  SWITCH_TO_SP(t0)
  sd s0,  SWITCH_TO_S0(t0)
  sd s1,  SWITCH_TO_S1(t0)
  sd s2,  SWITCH_TO_S2(t0)
  sd s3,  SWITCH_TO_S3(t0)
  sd s4,  SWITCH_TO_S4(t0)
  sd s5,  SWITCH_TO_S5(t0)
  sd s6,  SWITCH_TO_S6(t0)
  sd s7,  SWITCH_TO_S7(t0)
  sd s8,  SWITCH_TO_S8(t0)
  sd s9,  SWITCH_TO_S9(t0)
  sd s10, SWITCH_TO_S10(t0)
  sd s11, SWITCH_TO_S11(t0)
  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  /*restore next pcb*/ 
  move tp,a1
  ld t0,  PCB_KERNEL_SP(tp)/*now tp point to kernel stack*/
  ld ra,  SWITCH_TO_RA(t0)
  ld sp,  SWITCH_TO_SP(t0)
  ld s0,  SWITCH_TO_S0(t0)
  ld s1,  SWITCH_TO_S1(t0)
  ld s2,  SWITCH_TO_S2(t0)
  ld s3,  SWITCH_TO_S3(t0)
  ld s4,  SWITCH_TO_S4(t0)
  ld s5,  SWITCH_TO_S5(t0)
  ld s6,  SWITCH_TO_S6(t0)
  ld s7,  SWITCH_TO_S7(t0)
  ld s8,  SWITCH_TO_S8(t0)
  ld s9,  SWITCH_TO_S9(t0)
  ld s10, SWITCH_TO_S10(t0)
  ld s11, SWITCH_TO_S11(t0)

  /*addi sp, sp, SWITCH_TO_SIZE*/

  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  call unlock_kernel
  RESTORE_CONTEXT

  //debug: detect csr registers
  // csrr s8,CSR_SSTATUS
  // csrr s9,CSR_SCAUSE
  // csrr s10,CSR_STVAL
  //csrr s11,CSR_SEPC

  sret

  /* HINT: remember to check your sp, does it point to the right address? */
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)

  call lock_kernel
  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT

  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  la ra, ret_from_exception

  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  ld a0, PCB_KERNEL_SP(tp)
  csrr a1,CSR_STVAL
  csrr a2,CSR_SCAUSE
  jal zero,interrupt_helper

ENDPROC(exception_handler_entry)
