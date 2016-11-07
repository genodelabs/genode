/*
 * \brief  Default thread bootstrap code
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-04-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/env.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/native_utcb.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/utcb.h>
#include <l4/thread.h>
} }

Okl4::L4_ThreadId_t main_thread_tid;


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


void prepare_reinit_main_thread() { prepare_init_main_thread(); }


/************
 ** Thread **
 ************/

void Genode::Thread::_thread_bootstrap()
{
	native_thread().l4id.raw = Okl4::copy_uregister_to_utcb();
}


void Genode::Thread::_init_platform_thread(size_t, Type type)
{
	if (type == NORMAL) { return; }
	native_thread().l4id.raw = main_thread_tid.raw;
	_thread_cap   = env()->parent()->main_thread_cap();
}
