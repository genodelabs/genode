/*
 * \brief  Interface between kernel and userland
 * \author Adrian-Ken Rueegsegger
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <kernel/interface.h>

using namespace Kernel;

/************************************
 ** Helper macros for kernel calls **
 ************************************/

/**
 * Assign argument registers according to AMD64 parameter passing
 * convention to avoid additional register copy operations.
 */

#define CALL_1_FILL_ARG_REGS \
	register Call_arg arg_0_reg asm("rdi") = arg_0;

#define CALL_2_FILL_ARG_REGS \
	CALL_1_FILL_ARG_REGS \
	register Call_arg arg_1_reg asm("rsi") = arg_1;

#define CALL_3_FILL_ARG_REGS \
	CALL_2_FILL_ARG_REGS \
	register Call_arg arg_2_reg asm("rdx") = arg_2;

#define CALL_4_FILL_ARG_REGS \
	CALL_3_FILL_ARG_REGS \
	register Call_arg arg_3_reg asm("rcx") = arg_3;

#define CALL_5_FILL_ARG_REGS \
	CALL_4_FILL_ARG_REGS \
	register Call_arg arg_4_reg asm("r8") = arg_4;

#define CALL_6_FILL_ARG_REGS \
	CALL_5_FILL_ARG_REGS \
	register Call_arg arg_5_reg asm("r9") = arg_5;

#define CALL_1_SYSCALL "int $0x80\n" : "+r" (arg_0_reg)
#define CALL_2_SYSCALL CALL_1_SYSCALL: "r" (arg_1_reg)
#define CALL_3_SYSCALL CALL_2_SYSCALL, "r" (arg_2_reg)
#define CALL_4_SYSCALL CALL_3_SYSCALL, "r" (arg_3_reg)
#define CALL_5_SYSCALL CALL_4_SYSCALL, "r" (arg_4_reg)
#define CALL_6_SYSCALL CALL_5_SYSCALL, "r" (arg_5_reg)


/******************
 ** Kernel calls **
 ******************/

Call_ret Kernel::call(Call_arg arg_0)
{
	CALL_1_FILL_ARG_REGS
	asm volatile(CALL_1_SYSCALL);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1)
{
	CALL_2_FILL_ARG_REGS
	asm volatile(CALL_2_SYSCALL);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2)
{
	CALL_3_FILL_ARG_REGS
	asm volatile(CALL_3_SYSCALL);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3)
{
	CALL_4_FILL_ARG_REGS
	asm volatile(CALL_4_SYSCALL);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4)
{
	CALL_5_FILL_ARG_REGS
	asm volatile(CALL_5_SYSCALL);
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4,
                      Call_arg arg_5)
{
	CALL_6_FILL_ARG_REGS
	asm volatile(CALL_6_SYSCALL);
	return arg_0_reg;
}
