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


namespace Genode { void platform_main_bootstrap(); }


Genode::Native_thread_id main_thread_tid;


void Genode::platform_main_bootstrap()
{
	static struct Bootstrap
	{
		Bootstrap() { main_thread_tid = Kernel::current_thread_id(); }
	} bootstrap;
}
