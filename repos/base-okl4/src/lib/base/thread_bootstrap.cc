/*
 * \brief  Default thread bootstrap code
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-04-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/okl4.h>
#include <base/internal/stack.h>
#include <base/internal/runtime.h>

using namespace Genode;


Okl4::L4_ThreadId_t main_thread_tid;


void Genode::prepare_init_main_thread()
{
	/* copy thread ID to utcb */
	main_thread_tid.raw = Okl4::copy_uregister_to_utcb();
}


void Thread::_thread_bootstrap()
{
	with_native_thread([&] (Native_thread &nt) {
		nt.l4id.raw = Okl4::copy_uregister_to_utcb(); });
}


void Thread::_init_native_thread(Stack &) { }


void Thread::_init_native_main_thread(Stack &stack)
{
	stack.native_thread().l4id.raw = main_thread_tid.raw;
	_thread_cap = _runtime.parent.main_thread_cap();
}
