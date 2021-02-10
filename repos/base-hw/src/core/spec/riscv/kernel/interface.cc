/*
 * \brief  Direct kernel interface for core
 * \author Sebastian Sumpf
 * \date   2021-02-10
 *
 * System call bindings for privileged core threads. Core threads cannot use
 * hardware-system calls ('ecall') because machine mode (OpenSBI) will interpret
 * them as SBI calls from supvervisor mode (not system calls). Unknown SBI calls
 * will lead machine mode to either stopping the machine or doing the wrong thing.
 * In any case machine mode will not forward the 'ecall' to supervisor mode, it
 * does only so for 'ecall's from user land. Therefore, call the kernel
 * directly.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <kernel/interface.h>
#include <base/log.h>
#include <cpu/cpu_state.h>

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

extern Genode::addr_t _kernel_entry;

/*
 * Emulate RISC-V hardware-system call using jump ('jalr') instead of
 * environment call ('ecall').
 *
 * - clear SIE in sstatus (supervisor interrupt enable)
 * - set scause to ECALL from supervisor mode
 * - set sepc to "1:"
 * - jump to '_kernel_entry
 *
 * After system call execution continues at "1:"
 */
#define CALL_1_SWI "li   ra, 0x2    \n" \
                   "csrc sstatus, ra\n" \
                   "csrw scause, %2 \n" \
                   "la   %2, 1f     \n" \
                   "csrw sepc, %2   \n" \
                   "jalr %1         \n" \
                   "1:              \n" \
                   : "+r" (arg_0_reg) : "r" (&_kernel_entry), \
                     "r" (Genode::Cpu_state::ECALL_FROM_SUPERVISOR)
#define CALL_2_SWI CALL_1_SWI,  "r" (arg_1_reg)
#define CALL_3_SWI CALL_2_SWI,  "r" (arg_2_reg)
#define CALL_4_SWI CALL_3_SWI,  "r" (arg_3_reg)
#define CALL_5_SWI CALL_4_SWI,  "r" (arg_4_reg)


/******************
 ** Kernel calls **
 ******************/

Call_ret Kernel::call64(Call_arg arg_0)
{
	CALL_1_FILL_ARG_REGS
	asm volatile(CALL_1_SWI : "ra");
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0)
{
	CALL_1_FILL_ARG_REGS
	asm volatile(CALL_1_SWI : "ra");
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1)
{
	CALL_2_FILL_ARG_REGS
	asm volatile(CALL_2_SWI: "ra");
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2)
{
	CALL_3_FILL_ARG_REGS
	asm volatile(CALL_3_SWI : "ra");
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3)
{
	CALL_4_FILL_ARG_REGS
	asm volatile(CALL_4_SWI : "ra");
	return arg_0_reg;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4)
{
	CALL_5_FILL_ARG_REGS
	asm volatile(CALL_5_SWI : "ra");
	return arg_0_reg;
}
