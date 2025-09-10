/*
 * \brief  Thread bootstrap code
 * \author Christian Prochaska
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/pistachio.h>
#include <base/internal/runtime.h>


Pistachio::L4_ThreadId_t main_thread_tid;


void Genode::prepare_init_main_thread()
{
	main_thread_tid = Pistachio::L4_Myself();
}


void Genode::Thread::_thread_bootstrap()
{
	with_native_thread([&] (Native_thread &nt) {
		nt.l4id = Pistachio::L4_Myself(); });
}


void Genode::Thread::_init_native_thread(Stack &) { }


void Genode::Thread::_init_native_main_thread(Stack &stack)
{
	stack.native_thread().l4id = main_thread_tid;

	_thread_cap = _runtime.parent.main_thread_cap();
}
