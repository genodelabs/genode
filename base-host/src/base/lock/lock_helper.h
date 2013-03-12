/*
 * \brief  Dummy helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2009-10-02
 *
 * For documentation about the interface, please revisit the 'base-pistachio'
 * implementation.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>
#include <base/thread.h>


static inline void thread_yield() { }


static bool thread_check_stopped_and_restart(Genode::Thread_base *)
{
	return true;
}


static inline void thread_switch_to(Genode::Thread_base *)
{ }


static inline void thread_stop_myself() { while (true); }
