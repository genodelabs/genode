/*
 * \brief  Call main function  (X86 64 bit specific)
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2011-05-011
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _X86_64__CALL_MAIN_H_
#define _X86_64__CALL_MAIN_H_

/**
 * Restore SP from initial sp and jump to entry function
 */
void call_main(void (*func)(void))
{
	extern long __initial_sp;

	asm volatile ("movq %0, %%rsp;"
	              "jmpq *%1;"
	              : 
	              : "r" (__initial_sp),
	                "r" (func)
	              : "memory"
	             );
}

#endif /* _X86_64__CALL_MAIN_H_ */
