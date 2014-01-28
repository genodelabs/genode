/*
 * \brief  Thread initialization
 * \author Martin stein
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/env.h>

/* base-hw includes */
#include <kernel/interface.h>

using namespace Genode;

Ram_dataspace_capability _main_thread_utcb_ds;

Native_thread_id _main_thread_id;

namespace Genode { Rm_session * env_context_area_rm_session(); }


/**************************
 ** Native types support **
 **************************/

Native_thread_id Genode::thread_get_my_native_id()
{
	Thread_base * const t = Thread_base::myself();
	return t ? t->tid().thread_id : _main_thread_id;
}


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	using namespace Genode;

	/*
	 * Make data from the startup info persistantly available by copying it
	 * before the UTCB gets polluted by the following function calls.
	 */
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	_main_thread_id = utcb->start_info()->thread_id();
	_main_thread_utcb_ds =
		reinterpret_cap_cast<Ram_dataspace>(utcb->start_info()->utcb_ds());
}


void prepare_reinit_main_thread() { prepare_init_main_thread(); }


/*****************
 ** Thread_base **
 *****************/

void Thread_base::_thread_bootstrap()
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	_tid.thread_id = utcb->start_info()->thread_id();
}


void Thread_base::_init_platform_thread(Type type)
{
	/* nothing platform specific to do if this is not a special thread */
	if (type == NORMAL) { return; }

	/* if we got reinitialized we have to get rid of the old UTCB */
	size_t const utcb_size = sizeof(Native_utcb);
	addr_t const context_area = Native_config::context_area_virtual_base();
	addr_t const utcb_new = (addr_t)&_context->utcb - context_area;
	Rm_session * const rm = env_context_area_rm_session();
	if (type == REINITIALIZED_MAIN) { rm->detach(utcb_new); }

	/* remap initial main-thread UTCB according to context-area spec */
	try { rm->attach_at(_main_thread_utcb_ds, utcb_new, utcb_size); }
	catch(...) {
		PERR("failed to re-map UTCB");
		while (1) ;
	}
	/* adjust initial object state in case of a main thread */
	tid().thread_id = _main_thread_id;
}
