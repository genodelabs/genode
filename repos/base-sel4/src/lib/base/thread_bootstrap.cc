/*
 * \brief  Thread bootstrap code
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
#include <base/thread.h>
#include <base/env.h>

/* base-internal includes */
#include <base/internal/stack.h>


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread() { }


/************
 ** Thread **
 ************/

void Genode::Thread::_thread_bootstrap() 
{
	if (native_thread().ep_sel == 0) {
		native_thread().ep_sel = _stack->utcb().ep_sel;
		native_thread().lock_sel = _stack->utcb().lock_sel;
	}
}
