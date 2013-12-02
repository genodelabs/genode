/*
 * \brief  Testing thread environment of a main thread
 * \author Alexander Boettcher
 * \date   2013-12-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>

using namespace Genode;

int main(int argc, char **argv)
{
	/* check wether my thread object exists */
	Thread_base * myself = Genode::Thread_base::myself();
	if (!myself) { return -1; }
	printf("thread base          %p\n", myself);

	/* check wether my stack is inside the first context region */
	addr_t const context_base = Native_config::context_area_virtual_base();
	addr_t const context_size = Native_config::context_area_virtual_size();
	addr_t const context_top  = context_base + context_size;
	addr_t const stack_top  = (addr_t)myself->stack_top();
	addr_t const stack_base = (addr_t)myself->stack_base();
	if (stack_top  <= context_base) { return -2; }
	if (stack_top  >  context_top)  { return -3; }
	if (stack_base >= context_top)  { return -4; }
	if (stack_base <  context_base) { return -5; }
	printf("thread stack top     %p\n", myself->stack_top());
	printf("thread stack bottom  %p\n", myself->stack_base());

	/* check wether my stack pointer is inside my stack */
	unsigned dummy = 0;
	addr_t const sp = (addr_t)&dummy;
	if (sp >= stack_top)  { return -6; }
	if (sp <  stack_base) { return -7; }
	printf("thread stack pointer %p\n", (void *)sp);
	return 0;
}
