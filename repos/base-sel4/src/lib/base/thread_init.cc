/*
 * \brief  Implementation of Thread API interface
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/internal/native_thread.h>
#include <base/internal/capability_space_sel4.h>

using namespace Genode;


void Thread::_init_platform_thread(size_t, Type type)
{
	/**
	 * Reset to default values. The default values trigger initial allocations
	 * and associations the thread, like IPCbuffer in ipc.cc.
	 */
	native_thread() = Native_thread();

	if (type == MAIN) {
		native_thread().lock_sel = INITIAL_SEL_LOCK;
		return;
	}
}
