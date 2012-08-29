/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Martin Stein
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__PLATFORM___MAIN_HELPER_H_
#define _SRC__PLATFORM___MAIN_HELPER_H_

#include <base/native_types.h>
#include <kernel/config.h>
#include <base/thread.h>
#include <base/printf.h>

using namespace Genode;

extern Genode::Native_utcb* _main_utcb_addr;
extern Genode::Native_thread_id _main_thread_id;


Native_utcb* main_thread_utcb() { return _main_utcb_addr; }


static void main_thread_bootstrap()
{
	/*
	 * main thread has no Thread_base but he gets some informations about
	 * itself deposited by the programs parent
	 */

	/*
	 * If we're another mainthread than that of core we overwrite the
	 * utcb-address with that one genode takes by convention for mainthreads on
	 * microblaze
	 */
	int volatile pid;
	asm volatile ("mfs %0, rpid" : "=r"(pid) : :);
	if (pid!=Roottask::PROTECTION_ID) {
		_main_utcb_addr = (Native_utcb*)((Native_config::context_area_virtual_base()
		                + Native_config::context_area_virtual_size())
		                - sizeof(Native_utcb));
	}

	_main_thread_id=*((Native_thread_id*)main_thread_utcb());
}

#endif /* _SRC__PLATFORM___MAIN_HELPER_H_ */
