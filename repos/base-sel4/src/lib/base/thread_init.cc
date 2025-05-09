/*
 * \brief  Implementation of Thread API interface
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/internal/stack.h>
#include <base/internal/capability_space_sel4.h>

using namespace Genode;


void Thread::_init_native_thread(Stack &stack, size_t, Type type)
{
	/*
	 * Reset to default values. The default values trigger initial allocations
	 * and associations the thread, like IPCbuffer in ipc.cc.
	 */
	stack.native_thread().attr = { };

	if (type == MAIN)
		stack.native_thread().attr.lock_sel = INITIAL_SEL_LOCK;
}
