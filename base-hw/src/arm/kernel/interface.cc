/*
 * \brief  Interface between kernel and userland
 * \author Martin stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <kernel/interface.h>

using namespace Kernel;


/**********************************************************************
 ** Inline assembly templates for kernel calls with 1 to 6 arguments **
 **********************************************************************/

#define CALL_6_ASM_OPS \
	"mov r5, #0       \n" \
	"add r5, %[arg_5] \n" \
	CALL_5_ASM_OPS

#define CALL_5_ASM_OPS \
	"mov r4, #0       \n" \
	"add r4, %[arg_4] \n" \
	CALL_4_ASM_OPS

#define CALL_4_ASM_OPS \
	"mov r3, #0       \n" \
	"add r3, %[arg_3] \n" \
	CALL_3_ASM_OPS

#define CALL_3_ASM_OPS \
	"mov r2, #0       \n" \
	"add r2, %[arg_2] \n" \
	CALL_2_ASM_OPS

#define CALL_2_ASM_OPS \
	"mov r1, #0       \n" \
	"add r1, %[arg_1] \n" \
	CALL_1_ASM_OPS

#define CALL_1_ASM_OPS  \
	"mov r0, #0        \n" \
	"add r0, %[arg_0]  \n" \
	"swi 0             \n" \
	"mov %[result], #0 \n" \
	"add %[result], r0   "


/****************************************************************************
 ** Inline assembly "writeable" tpl-args for kernel calls with 1 to 6 args **
 ****************************************************************************/

#define CALL_6_ASM_WRITE [arg_5] "+r" (arg_5), CALL_5_ASM_WRITE
#define CALL_5_ASM_WRITE [arg_4] "+r" (arg_4), CALL_4_ASM_WRITE
#define CALL_4_ASM_WRITE [arg_3] "+r" (arg_3), CALL_3_ASM_WRITE
#define CALL_3_ASM_WRITE [arg_2] "+r" (arg_2), CALL_2_ASM_WRITE
#define CALL_2_ASM_WRITE [arg_1] "+r" (arg_1), CALL_1_ASM_WRITE
#define CALL_1_ASM_WRITE \
	[arg_0]  "+r" (arg_0),  \
	[result] "+r" (result)


/**************************************************************************
 ** Inline assembly clobber lists for kernel calls with 1 to 6 arguments **
 **************************************************************************/

#define CALL_6_ASM_CLOBBER "r5", CALL_5_ASM_CLOBBER
#define CALL_5_ASM_CLOBBER "r4", CALL_4_ASM_CLOBBER
#define CALL_4_ASM_CLOBBER "r3", CALL_3_ASM_CLOBBER
#define CALL_3_ASM_CLOBBER "r2", CALL_2_ASM_CLOBBER
#define CALL_2_ASM_CLOBBER "r1", CALL_1_ASM_CLOBBER
#define CALL_1_ASM_CLOBBER "r0"


/************************************
 ** Calls with 1 to 6 arguments **
 ************************************/

Call_ret Kernel::call(Call_arg arg_0)
{
	Call_ret result = 0;
	asm volatile(CALL_1_ASM_OPS
	           : CALL_1_ASM_WRITE
	          :: CALL_1_ASM_CLOBBER);
	return result;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1)
{
	Call_ret result = 0;
	asm volatile(CALL_2_ASM_OPS
	           : CALL_2_ASM_WRITE
	          :: CALL_2_ASM_CLOBBER);
	return result;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2)
{
	Call_ret result = 0;
	asm volatile(CALL_3_ASM_OPS
	           : CALL_3_ASM_WRITE
	          :: CALL_3_ASM_CLOBBER);
	return result;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3)
{
	Call_ret result = 0;
	asm volatile(CALL_4_ASM_OPS
	           : CALL_4_ASM_WRITE
	          :: CALL_4_ASM_CLOBBER);
	return result;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4)
{
	Call_ret result = 0;
	asm volatile(CALL_5_ASM_OPS
	           : CALL_5_ASM_WRITE
	          :: CALL_5_ASM_CLOBBER);
	return result;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4,
                      Call_arg arg_5)
{
	Call_ret result = 0;
	asm volatile(CALL_6_ASM_OPS
	           : CALL_6_ASM_WRITE
	          :: CALL_6_ASM_CLOBBER);
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
