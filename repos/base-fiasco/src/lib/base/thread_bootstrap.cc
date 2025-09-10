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

/* base-internal includes */
#include <base/internal/runtime.h>


void Genode::prepare_init_main_thread()  { }
void Genode::Thread::_thread_bootstrap() { }
void Genode::Thread::_init_native_thread(Stack &) { }


void Genode::Thread::_init_native_main_thread(Stack &)
{
	_thread_cap = _runtime.parent.main_thread_cap();
}
