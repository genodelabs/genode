/*
 * \brief  Syscall-framework implementation for ARM
 * \author Martin stein
 * \date   2011-11-30
 *
 * The code in this file is compliant to the general ARM instruction- and
 * register-set but the semantics are tested only on ARMv6 and ARMv7 by now.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <kernel/syscalls.h>

using namespace Kernel;


/******************************************************************
 ** Inline assembly templates for syscalls with 1 to 6 arguments **
 ******************************************************************/

#define SYSCALL_6_ASM_OPS \
	"mov r5, #0       \n" \
	"add r5, %[arg_5] \n" \
	SYSCALL_5_ASM_OPS

#define SYSCALL_5_ASM_OPS \
	"mov r4, #0       \n" \
	"add r4, %[arg_4] \n" \
	SYSCALL_4_ASM_OPS

#define SYSCALL_4_ASM_OPS \
	"mov r3, #0       \n" \
	"add r3, %[arg_3] \n" \
	SYSCALL_3_ASM_OPS

#define SYSCALL_3_ASM_OPS \
	"mov r2, #0       \n" \
	"add r2, %[arg_2] \n" \
	SYSCALL_2_ASM_OPS

#define SYSCALL_2_ASM_OPS \
	"mov r1, #0       \n" \
	"add r1, %[arg_1] \n" \
	SYSCALL_1_ASM_OPS

#define SYSCALL_1_ASM_OPS  \
	"mov r0, #0        \n" \
	"add r0, %[arg_0]  \n" \
	"swi 0             \n" \
	"mov %[result], #0 \n" \
	"add %[result], r0   "


/*****************************************************************************
 ** Inline assembly "writeable" template-args for syscalls with 1 to 6 args **
 *****************************************************************************/

#define SYSCALL_6_ASM_WRITE [arg_5] "+r" (arg_5), SYSCALL_5_ASM_WRITE
#define SYSCALL_5_ASM_WRITE [arg_4] "+r" (arg_4), SYSCALL_4_ASM_WRITE
#define SYSCALL_4_ASM_WRITE [arg_3] "+r" (arg_3), SYSCALL_3_ASM_WRITE
#define SYSCALL_3_ASM_WRITE [arg_2] "+r" (arg_2), SYSCALL_2_ASM_WRITE
#define SYSCALL_2_ASM_WRITE [arg_1] "+r" (arg_1), SYSCALL_1_ASM_WRITE
#define SYSCALL_1_ASM_WRITE \
	[arg_0]  "+r" (arg_0),  \
	[result] "+r" (result)


/**********************************************************************
 ** Inline assembly clobber lists for syscalls with 1 to 6 arguments **
 **********************************************************************/

#define SYSCALL_6_ASM_CLOBBER "r5", SYSCALL_5_ASM_CLOBBER
#define SYSCALL_5_ASM_CLOBBER "r4", SYSCALL_4_ASM_CLOBBER
#define SYSCALL_4_ASM_CLOBBER "r3", SYSCALL_3_ASM_CLOBBER
#define SYSCALL_3_ASM_CLOBBER "r2", SYSCALL_2_ASM_CLOBBER
#define SYSCALL_2_ASM_CLOBBER "r1", SYSCALL_1_ASM_CLOBBER
#define SYSCALL_1_ASM_CLOBBER "r0"


/************************************
 ** Syscalls with 1 to 6 arguments **
 ************************************/

Syscall_ret Kernel::syscall(Syscall_arg arg_0)
{
	Syscall_ret result = 0;
	asm volatile(SYSCALL_1_ASM_OPS
	           : SYSCALL_1_ASM_WRITE
	          :: SYSCALL_1_ASM_CLOBBER);
	return result;
}


Syscall_ret Kernel::syscall(Syscall_arg arg_0,
                            Syscall_arg arg_1)
{
	Syscall_ret result = 0;
	asm volatile(SYSCALL_2_ASM_OPS
	           : SYSCALL_2_ASM_WRITE
	          :: SYSCALL_2_ASM_CLOBBER);
	return result;
}


Syscall_ret Kernel::syscall(Syscall_arg arg_0,
                            Syscall_arg arg_1,
                            Syscall_arg arg_2)
{
	Syscall_ret result = 0;
	asm volatile(SYSCALL_3_ASM_OPS
	           : SYSCALL_3_ASM_WRITE
	          :: SYSCALL_3_ASM_CLOBBER);
	return result;
}


Syscall_ret Kernel::syscall(Syscall_arg arg_0,
                            Syscall_arg arg_1,
                            Syscall_arg arg_2,
                            Syscall_arg arg_3)
{
	Syscall_ret result = 0;
	asm volatile(SYSCALL_4_ASM_OPS
	           : SYSCALL_4_ASM_WRITE
	          :: SYSCALL_4_ASM_CLOBBER);
	return result;
}


Syscall_ret Kernel::syscall(Syscall_arg arg_0,
                            Syscall_arg arg_1,
                            Syscall_arg arg_2,
                            Syscall_arg arg_3,
                            Syscall_arg arg_4)
{
	Syscall_ret result = 0;
	asm volatile(SYSCALL_5_ASM_OPS
	           : SYSCALL_5_ASM_WRITE
	          :: SYSCALL_5_ASM_CLOBBER);
	return result;
}


Syscall_ret Kernel::syscall(Syscall_arg arg_0,
                            Syscall_arg arg_1,
                            Syscall_arg arg_2,
                            Syscall_arg arg_3,
                            Syscall_arg arg_4,
                            Syscall_arg arg_5)
{
	Syscall_ret result = 0;
	asm volatile(SYSCALL_6_ASM_OPS
	           : SYSCALL_6_ASM_WRITE
	          :: SYSCALL_6_ASM_CLOBBER);
	return result;
}


/*************************
 ** CPU-state utilities **
 *************************/

typedef Thread_reg_id Reg_id;

static addr_t const _cpu_state_regs[] = {
	Reg_id::R0,   Reg_id::R1,   Reg_id::R2,  Reg_id::R3, Reg_id::R4,
	Reg_id::R5,   Reg_id::R6,   Reg_id::R7,  Reg_id::R8, Reg_id::R9,
	Reg_id::R10,  Reg_id::R11,  Reg_id::R12, Reg_id::SP, Reg_id::LR,
	Reg_id::IP,   Reg_id::CPSR, Reg_id::CPU_EXCEPTION };

addr_t const * cpu_state_regs() { return _cpu_state_regs; }


size_t cpu_state_regs_length()
{
	return sizeof(_cpu_state_regs)/sizeof(_cpu_state_regs[0]);
}
