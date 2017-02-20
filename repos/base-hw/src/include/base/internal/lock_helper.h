/*
 * \brief  Helper functions for the lock implementation
 * \author Martin Stein
 * \date   2011-01-02
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/capability_space.h>

namespace Hw { extern Genode::Untyped_capability _main_thread_cap; }


/**
 * Yield execution time-slice of current thread
 */
static inline void thread_yield() { Kernel::yield_thread(); }


/**
 * Return kernel name of thread t
 */
static inline Kernel::capid_t
native_thread_id(Genode::Thread * const t)
{
	using Genode::Capability_space::capid;
	return t ? capid(t->native_thread().cap) : capid(Hw::_main_thread_cap);
}


/**
 * Yield execution time-slice of current thread to thread t
 */
static inline void thread_switch_to(Genode::Thread * const t)
{
	Kernel::yield_thread();
}


/**
 * Resume thread t and return wether t was paused or not
 */
static inline bool
thread_check_stopped_and_restart(Genode::Thread * const t)
{
	return Kernel::restart_thread(native_thread_id(t));
}


/**
 * Pause execution of current thread
 */
static inline void thread_stop_myself() { Kernel::stop_thread(); }


#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
