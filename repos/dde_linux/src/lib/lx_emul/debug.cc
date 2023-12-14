/*
 * \brief  Lx_emul backend for Linux kernel' debug functions
 * \author Stefan Kalkowski
 * \author Christian Helmuth
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

using namespace Genode;

extern "C" void lx_emul_trace_and_stop(const char * func)
{
	error("Function ", func, " not implemented yet!");
	backtrace();
	log("Will sleep forever...");
	sleep_forever();
}


extern "C" void lx_emul_trace(const char *) {}


extern "C" void lx_emul_backtrace()
{
	backtrace();
}
