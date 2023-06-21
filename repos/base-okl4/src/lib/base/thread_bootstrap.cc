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
#include <base/env.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/native_utcb.h>
#include <base/internal/globals.h>
#include <base/internal/okl4.h>

using namespace Genode;


Okl4::L4_ThreadId_t main_thread_tid;


static Thread_capability main_thread_cap(Thread_capability main_cap = { })
{
	static Thread_capability cap = main_cap;
	return cap;
}


/*******************
 ** local helpers **
 *******************/

namespace Okl4
{
	/*
	 * Read global thread ID from user-defined handle and store it
	 * into a designated UTCB entry.
	 */
	L4_Word_t copy_uregister_to_utcb()
	{
		using namespace Okl4;

		L4_Word_t my_global_id = L4_UserDefinedHandle();
		__L4_TCR_Set_ThreadWord(Genode::UTCB_TCR_THREAD_WORD_MYSELF,
		                        my_global_id);
		return my_global_id;
	}
}

using namespace Genode;


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	/* copy thread ID to utcb */
	main_thread_tid.raw = Okl4::copy_uregister_to_utcb();

	/* adjust main-thread ID if this is the main thread of core */
	if (main_thread_tid.raw == 0) {
		main_thread_tid.raw = Okl4::L4_rootserver.raw;
	}
}


/************
 ** Thread **
 ************/

void Thread::_thread_bootstrap()
{
	native_thread().l4id.raw = Okl4::copy_uregister_to_utcb();
}


void Thread::_init_platform_thread(size_t, Type type)
{
	if (type == NORMAL)
		return;

	native_thread().l4id.raw = main_thread_tid.raw;
	_thread_cap = main_thread_cap();
}


void Genode::init_thread_bootstrap(Cpu_session &, Thread_capability main_cap)
{
	main_thread_cap(main_cap);
}
