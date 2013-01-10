/*
 * \brief  Helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2010-04-20
 *
 * For documentation about the interface, please revisit the 'base-pistachio'
 * implementation.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>
#include <base/thread.h>

/* Codezero includes */
#include <codezero/syscalls.h>


static Codezero::l4_mutex main_running_lock = { -1 };


static inline void thread_yield()
{
	Codezero::l4_thread_switch(-1);
}


static inline bool thread_id_valid(Genode::Native_thread_id tid)
{
	return tid.tid != Codezero::NILTHREAD;
}


static inline bool thread_check_stopped_and_restart(Genode::Native_thread_id tid)
{
	if (!thread_id_valid(tid))
		return false;

	Codezero::l4_mutex_unlock(tid.running_lock);
	return true;
}


static inline Genode::Native_thread_id thread_get_my_native_id()
{
	using namespace Genode;

	Codezero::l4_mutex *running_lock = 0;

	/* obtain pointer to running lock of calling thread */
	if (Thread_base::myself())
		running_lock = Thread_base::myself()->utcb()->running_lock();
	else {
		running_lock = &main_running_lock;
		if (running_lock->lock == -1) {
			Codezero::l4_mutex_init(running_lock);
			Codezero::l4_mutex_lock(running_lock); /* block on first mutex lock */
		}
	}

	return Genode::Native_thread_id(Codezero::thread_myself(), running_lock);
}


static inline Genode::Native_thread_id thread_invalid_id()
{
	return Genode::Native_thread_id(Codezero::NILTHREAD, 0);
}


static inline void thread_switch_to(Genode::Native_thread_id tid)
{
	if (thread_id_valid(tid))
		Codezero::l4_thread_switch(tid.tid);
}


static inline void thread_stop_myself()
{
	Genode::Native_thread_id myself = thread_get_my_native_id();
	Codezero::l4_mutex_lock(myself.running_lock);
}
