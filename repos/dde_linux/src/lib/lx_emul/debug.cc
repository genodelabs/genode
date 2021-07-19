/*
 * \brief  Lx_emul backend for Linux kernel' debug functions
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <base/sleep.h>
#include <os/backtrace.h>

#include <lx_emul/debug.h>

extern "C" void lx_emul_trace_and_stop(const char * func)
{
	using namespace Genode;

	error("Function ", func, " not implemented yet!");
	log("Backtrace follows:");
	backtrace();
	log("Will sleep forever...");
	sleep_forever();
}


extern "C" void lx_emul_trace(const char *) {}
