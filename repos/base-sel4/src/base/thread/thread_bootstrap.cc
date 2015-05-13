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


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread() { }

void prepare_reinit_main_thread() { prepare_init_main_thread(); }


/*****************
 ** Thread_base **
 *****************/

void Genode::Thread_base::_thread_bootstrap() 
{
	if (tid().ep_sel == 0) {
		tid().ep_sel = _context->utcb.ep_sel;
	}
}
