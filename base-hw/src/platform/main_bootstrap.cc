/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Martin Stein
 * \author Christian Helmuth
 * \date   2010-09-13
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

using namespace Genode;

namespace Genode { void platform_main_bootstrap(); }

Native_thread_id _main_thread_id;


Native_thread_id Genode::thread_get_my_native_id()
{
	Thread_base * const t = Thread_base::myself();
	return t ? t->tid().thread_id : _main_thread_id;
}


void Genode::platform_main_bootstrap()
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	_main_thread_id = utcb->startup_msg.thread_id();
}
