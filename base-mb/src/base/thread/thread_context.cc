/*
 * \brief  Thread-context specific part of the thread library
 * \author Norman Feske
 * \date   2010-01-19
 *
 * This part of the thread library is required by the IPC framework
 * also if no threads are used.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

/* kernel includes */
#include <kernel/syscalls.h>
#include <base/capability.h>
#include <xilinx/microblaze.h>

using namespace Genode;


extern Genode::Native_utcb* _main_utcb_addr;
Genode::Native_thread_id _main_thread_id;


bool is_this_main_thread() { return Thread_base::myself() == 0; }


Native_utcb* Thread_base::utcb()
{
	if (is_this_main_thread())
		return _main_utcb_addr;

	return &_context->utcb;
}


Native_thread_id Genode::my_thread_id()
{
	if (!is_this_main_thread())
		return Thread_base::myself()->tid();

	unsigned pid = (unsigned)Xilinx::Microblaze::protection_id();

	if (pid == Roottask::PROTECTION_ID)
		return Roottask::MAIN_THREAD_ID;

	return _main_thread_id;
}


