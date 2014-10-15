/*
 * \brief  seL4-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>
#include <base/thread.h>


static inline void thread_yield() { }


static inline bool thread_check_stopped_and_restart(Genode::Thread_base *)
{
	return false;
}


static inline void thread_switch_to(Genode::Thread_base *thread_base) { }


static inline void thread_stop_myself() { }
