/*
 * \brief  Interface between kernel and userland
 * \author Martin Stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
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


/******************
 ** Kernel calls **
 ******************/

Call_ret Kernel::call(Call_arg arg_0)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4,
                      Call_arg arg_5)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}
