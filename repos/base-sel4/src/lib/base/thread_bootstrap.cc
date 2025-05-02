/*
 * \brief  Thread bootstrap code
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/env.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/globals.h>


/*****************************
 ** Startup library support **
 *****************************/

void Genode::prepare_init_main_thread() { }


/************
 ** Thread **
 ************/

void Genode::Thread::_thread_bootstrap()
{
	_stack.with_result(
		[&] (Stack &stack) {
			Native_thread &nt = stack.native_thread();

			if (nt.attr.ep_sel == 0) {
				nt.attr.ep_sel   = (unsigned)stack.utcb().ep_sel();
				nt.attr.lock_sel = (unsigned)stack.utcb().lock_sel();
			}
		},
		[&] (Stack_error) { }
	);
}


void Genode::init_thread_bootstrap(Cpu_session &, Thread_capability) { }
