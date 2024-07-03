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
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/output.h>
#include <base/internal/raw_write_string.h>

/* core includes */
#include <core_log.h>
#include <hw/memory_map.h>
#include <kernel/log.h>

using namespace Core;


void Core_log::out(char const c) { Kernel::log(c); }


void Genode::raw_write_string(char const *str)
{
	while (char c = *str++)
		Kernel::log(c);
}


/************************************************************
 ** Utility to check whether kernel or core code is active **
 ************************************************************/

static inline bool running_in_kernel()
{
	Hw::Memory_region const cpu_region = Hw::Mm::cpu_local_memory();

	/* check stack variable against kernel's cpu local memory area */
	return ((addr_t)&cpu_region) >= cpu_region.base &&
	       ((addr_t)&cpu_region) < cpu_region.end();
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
