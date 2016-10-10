/*
 * \brief  Interface between kernel and userland
 * \author Sebastian Sumpf
 * \date   2015-06-02
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <kernel/interface.h>

using namespace Kernel;


/************************************
 ** Helper macros for kernel calls **
 ************************************/

#define CALL_1_FILL_ARG_REGS \
	register Call_arg arg_0_reg asm("a0") = arg_0;

#define CALL_2_FILL_ARG_REGS \
	CALL_1_FILL_ARG_REGS \
	register Call_arg arg_1_reg asm("a1") = arg_1;

#define CALL_3_FILL_ARG_REGS \
	CALL_2_FILL_ARG_REGS \
	register Call_arg arg_2_reg asm("a2") = arg_2;

#define CALL_4_FILL_ARG_REGS \
	CALL_3_FILL_ARG_REGS \
	register Call_arg arg_3_reg asm("a3") = arg_3;

#define CALL_5_FILL_ARG_REGS \
	CALL_4_FILL_ARG_REGS \
	register Call_arg arg_4_reg asm("a4") = arg_4;

#define CALL_1_SWI "ecall\n" : "+r" (arg_0_reg)
#define CALL_2_SWI CALL_1_SWI:  "r" (arg_1_reg)
#define CALL_3_SWI CALL_2_SWI,  "r" (arg_2_reg)
#define CALL_4_SWI CALL_3_SWI,  "r" (arg_3_reg)
#define CALL_5_SWI CALL_4_SWI,  "r" (arg_4_reg)


/******************
 ** Kernel calls **
 ******************/

Call_ret Kernel::call(Call_arg arg_0)
{
	CALL_1_FILL_ARG_REGS
	asm volatile(CALL_1_SWI);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1)
{
	CALL_2_FILL_ARG_REGS
	asm volatile(CALL_2_SWI);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2)
{
	CALL_3_FILL_ARG_REGS
	asm volatile(CALL_3_SWI);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3)
{
	CALL_4_FILL_ARG_REGS
	asm volatile(CALL_4_SWI);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4)
{
	CALL_5_FILL_ARG_REGS
	asm volatile(CALL_5_SWI);
	return arg_0_reg;
}
