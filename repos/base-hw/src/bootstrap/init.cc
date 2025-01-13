/*
 * \brief   Initialization code for bootstrap
 * \author  Stefan Kalkowski
 * \date    2016-09-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local */
#include <platform.h>

/* base includes */
#include <base/internal/globals.h>

using namespace Genode;

static constexpr size_t STACK_SIZE = 0x2000;

size_t  bootstrap_stack_size = STACK_SIZE;
uint8_t bootstrap_stack[Board::NR_OF_CPUS][STACK_SIZE]
__attribute__((aligned(get_page_size())));


Bootstrap::Platform & Bootstrap::platform()
{
	/*
	 * Don't use static local variable because cmpxchg cannot be executed
	 * w/o MMU on ARMv6.
	 */
	static long _obj[(sizeof(Bootstrap::Platform)+sizeof(long))/sizeof(long)];
	static Bootstrap::Platform *ptr;
	if (!ptr)
		ptr = construct_at<Bootstrap::Platform>(_obj);

	return *ptr;
}


extern "C" void init() __attribute__ ((noreturn));
extern "C" void init()
{
	Bootstrap::Platform & p = Bootstrap::platform();
	p.start_core(p.enable_mmu());
}
