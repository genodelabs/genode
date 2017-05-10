/*
 * \brief  Platform specific thread initialization
 * \author Martin Stein
 * \date   2014-01-06
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
#include <base/internal/globals.h>

using namespace Genode;


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()   { }
void prepare_reinit_main_thread() { }


/************
 ** Thread **
 ************/

void Thread::_thread_bootstrap() { }


void Thread::_init_platform_thread(size_t, Type type)
{
	if (type == NORMAL) return;

	_thread_cap = main_thread_cap();
}
