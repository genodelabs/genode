/*
 * \brief  Information about the main thread
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>
#include <base/cap_sel_alloc.h>
#include <base/printf.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;

/**
 * Location of the main thread's UTCB, initialized by the startup code
 */
Nova::mword_t __main_thread_utcb;


Native_utcb *main_thread_utcb() { return (Native_utcb *)__main_thread_utcb; }


int main_thread_running_semaphore()
{
	static int sm;
	if (!sm) {
		sm = cap_selector_allocator()->alloc();
		int res = Nova::create_sm(sm, Cap_selector_allocator::pd_sel(), 0);
		if (res)
			PERR("create_sm returned %d", res);
	}
	return sm;
}
