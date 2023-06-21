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
#include <base/env.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/globals.h>
#include <base/internal/pistachio.h>

using namespace Genode;


Pistachio::L4_ThreadId_t main_thread_tid;


static Thread_capability main_thread_cap(Thread_capability main_cap = { })
{
	static Thread_capability cap = main_cap;
	return cap;
}


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	main_thread_tid = Pistachio::L4_Myself();
}


/************
 ** Thread **
 ************/

void Genode::Thread::_thread_bootstrap()
{
	native_thread().l4id = Pistachio::L4_Myself();
}


void Genode::Thread::_init_platform_thread(size_t, Type type)
{
	if (type == NORMAL)
		return;

	native_thread().l4id = main_thread_tid;

	_thread_cap = main_thread_cap();
}


void Genode::init_thread_bootstrap(Cpu_session &, Thread_capability main_cap)
{
	main_thread_cap(main_cap);
}
