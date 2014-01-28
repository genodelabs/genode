/*
 * \brief  Call main function  (X86 specific)
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \author Martin Stein
 * \date   2011-05-02
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _X86_32__CALL_MAIN_H_
#define _X86_32__CALL_MAIN_H_

void * my_stack_top();
void set_program_var(const char *, const void *);

extern void * __initial_sp;
extern void * __initial_ax;
extern void * __initial_di;

/**
 * Call program _main with the environment that its CRT0 would have created
 *
 * \param _main_fp  pointer to _main function of dynamic program
 */
void call_main(void (*_main_fp)(void))
{
	/* make initial value of some registers available to dynamic program */
	set_program_var("__initial_sp", __initial_sp);
	set_program_var("__initial_ax", __initial_ax);
	set_program_var("__initial_di", __initial_di);

	/*
	 * We could also do a call but that would enable the the program main to
	 * return to LDSO wich isn't desired. This means also that not resetting
	 * the SP to stack top as we do would waste stack memory for dead LDSO
	 * frames.
	 */
	asm volatile ("mov %[sp], %%esp;"
	              "xor %%ebp, %%ebp;"
	              "jmp *%[ip];"
	              :: [sp] "r" (my_stack_top()),
	                 [ip] "r" (_main_fp)
	              : "memory");
}

#endif /* _X86_32__CALL_MAIN_H_ */
