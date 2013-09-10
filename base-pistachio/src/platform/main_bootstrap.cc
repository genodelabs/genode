/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/thread.h>
}


namespace Genode { void platform_main_bootstrap(); }


Genode::Native_thread_id main_thread_tid;


void Genode::platform_main_bootstrap()
{
	static struct Bootstrap
	{
		Bootstrap() { main_thread_tid = Pistachio::L4_Myself(); }
	} bootstrap;
}
