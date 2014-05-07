/*
 * \brief  Thread initialization
 * \author Martin stein
 * \author Stefan Kalkowski
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
#include <base/sleep.h>

/* base-hw includes */
#include <kernel/interface.h>

using namespace Genode;

Ram_dataspace_capability _main_thread_utcb_ds;
Native_thread_id         _main_thread_id;


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

extern Native_utcb* main_thread_utcb();

Native_utcb * Thread_base::utcb()
{
	if (this) { return &_context->utcb; }
	return main_thread_utcb();
}


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	Thread_base::myself()->_join_lock.unlock();
	Genode::sleep_forever();
}

void Thread_base::_thread_bootstrap()
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	_tid.thread_id = utcb->start_info()->thread_id();
}
