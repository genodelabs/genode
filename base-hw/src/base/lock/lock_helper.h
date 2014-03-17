/*
 * \brief  Helper functions for the lock implementation
 * \author Martin Stein
 * \date   2011-01-02
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOCK_HELPER_H_
#define _LOCK_HELPER_H_

/* Genode includes */
#include <base/native_types.h>
#include <base/thread.h>

extern Genode::Native_thread_id _main_thread_id;


/**
 * Yield execution time-slice of current thread
 */
static inline void thread_yield() { Kernel::yield_thread(0); }


/**
 * Return kernel name of thread t
 */
static inline Genode::Native_thread_id
native_thread_id(Genode::Thread_base * const t)
{
	return t ? t->tid().thread_id : _main_thread_id;
}


/**
 * Yield execution time-slice of current thread to thread t
 */
static inline void thread_switch_to(Genode::Thread_base * const t)
{
	Kernel::yield_thread(native_thread_id(t));
}


/**
 * Resume thread t and return wether t was paused or not
 */
static inline bool
thread_check_stopped_and_restart(Genode::Thread_base * const t)
{
	return Kernel::resume_thread(native_thread_id(t));
}


/**
 * Pause execution of current thread
 */
static inline void thread_stop_myself() { Kernel::pause_current_thread(); }


#endif /* _LOCK_HELPER_H_ */

