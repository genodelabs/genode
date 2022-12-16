/*
 * \brief  Access to the core's log facility
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/output.h>
#include <base/internal/raw_write_string.h>

#include <core_log.h>
#include <kernel/cpu.h>
#include <kernel/log.h>

using namespace Genode;


void Core_log::out(char const c) { Kernel::log(c); }


void Genode::raw_write_string(char const *str)
{
	while (char c = *str++)
		Kernel::log(c);
}


/************************************************************
 ** Utility to check whether kernel or core code is active **
 ************************************************************/

extern void const * const kernel_stack;

static inline bool running_in_kernel()
{
	addr_t const stack_base = reinterpret_cast<addr_t>(&kernel_stack);
	static constexpr size_t stack_size =
		NR_OF_CPUS * Kernel::Cpu::KERNEL_STACK_SIZE;

	/* check stack variable against kernel stack area */
	return ((addr_t)&stack_base) >= stack_base &&
	       ((addr_t)&stack_base) < (stack_base + stack_size);
}


/*******************************************
 ** Implementation of src/lib/base/log.cc **
 *******************************************/

void Log::_acquire(Type type)
{
	if (!running_in_kernel()) _mutex.acquire();

	/*
	 * Mark warnings and errors via distinct colors.
	 */
	switch (type) {
	case LOG:                                              break;
	case WARNING: _output.out_string("\033[34mWarning: "); break;
	case ERROR:   _output.out_string("\033[31mError: ");   break;
	};
}


void Log::_release()
{
	/*
	 * Reset color and add newline
	 */
	_output.out_string("\033[0m\n");

	if (!running_in_kernel()) _mutex.release();
}


void Raw::_acquire()
{
	/*
	 * Mark raw output with distinct color
	 */
	_output().out_string("\033[32mKernel: ");
}


void Raw::_release()
{
	/*
	 * Reset color and add newline
	 */
	_output().out_string("\033[0m\n");
}


void Trace_output::Write_trace_fn::operator () (char const *s)
{
	Thread::trace(s);
}
