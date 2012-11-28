/*
 * \brief   Simulated Atomic Operations on Xilinx Microblaze
 * \author  Martin Stein
 * \date    2010-08-30
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.global _atomic_cmpxchg


/**
 * the virtual area to the following code is interrupt save and 
 * executable-only in virtual mode. the code have to be position 
 * independent, so it can be linked to the according virtual area 
 * in any image that is executed above kernel to import the labels.
 * it is bothsides aligned - so that no common functions could gain
 * interruptsave status because it were linked inside this page too
 */
.section ".Atomic_ops"
.global _atomic_ops_begin
.align 12
_atomic_ops_begin:

/**
 * Atomic compare and exchange, see cmpxchg
 * 
 * should only be used wrapped by cmpxchg
 * otherwise this operation may won't behave as wished
 * and leads to unnesscessary yielding of thread!
 *
 * Parameters dest, cmp_val, new_val and *dest are assumed in r30, r29, r28 and r27
 * R15 is assumed to hold return pointer 
 * Return value is stored in r28
 */
_atomic_cmpxchg:

	xor r29, r29, r27                    /* diff=cmp_val-*dest    */
	bnei r29, _atomic_cmpxchg_notequal_1 /* if(!diff) {           */
	swi r28, r30, 0                      /*   *dest=new_val       */
	_atomic_cmpxchg_notequal_1:          /* }                     */
	or r28, r0, r0                       /* result=0              */
	bnei r29, _atomic_cmpxchg_notequal_2 /* if(!diff) {           */
	addi r28, r0, 1                      /*   result=1            */
	_atomic_cmpxchg_notequal_2:          /* }                     */

/* idle a while in interrupt save state, enable this for testing atomicity of atomic ops */
/*
	addi r29, r0, 0x04000000
	_atomic_cmpxchg_idle:
	addi r29, r29, -1
	bnei r29, _atomic_cmpxchg_idle
*/

	beqi r31, _atomic_cmpxchg_yield_return  /* if(interrupt occured) { */
	bri _atomic_syscall_yield               /*   yield thread          */
	_atomic_cmpxchg_yield_return:           /* }else{                  */
	rtsd r15, +2*4                          /*   return to nonatomic   */
	or r0, r0, r0                           /* }                       */


/**
 * Yield Thread (for atomic ops, if interrupt has occured during execution)
 */
_atomic_syscall_yield:

	/* backup registers */
	addik  r1, r1, -2*4
	swi   r15, r1, +0*4
	swi   r31, r1, +1*4

	/* set syscall type (see include/kernel/syscalls.h) */
	addi r31, r0, 7

	 /* jump to cpus syscall-handler if it's enabled */
	brki r15, 0x8
	or r0, r0, r0

	 /* recover & return */
	lwi   r15, r1, +0*4
	lwi   r31, r1, +1*4
	addik  r1, r1, +2*4
	bri _atomic_cmpxchg_yield_return

.global _atomic_ops_end
.align 12
_atomic_ops_end:

