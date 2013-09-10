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

/* OKL4-specific includes and definitions */
namespace Okl4 { extern "C" {
#include <l4/utcb.h>
#include <l4/thread.h>
} }


namespace Okl4 {

	/*
	 * Read global thread ID from user-defined handle and store it
	 * into a designated UTCB entry.
	 */
	L4_Word_t copy_uregister_to_utcb()
	{
		using namespace Okl4;

		L4_Word_t my_global_id = L4_UserDefinedHandle();
		__L4_TCR_Set_ThreadWord(Genode::UTCB_TCR_THREAD_WORD_MYSELF,
		                        my_global_id);
		return my_global_id;
	}
}


namespace Genode { void platform_main_bootstrap(); }


Genode::Native_thread_id main_thread_tid;


void Genode::platform_main_bootstrap()
{
	static struct Bootstrap
	{
		Bootstrap()
		{
			/* copy thread ID to utcb */
			main_thread_tid.raw = Okl4::copy_uregister_to_utcb();

			if (main_thread_tid.raw == 0) /* core */
				main_thread_tid.raw = Okl4::L4_rootserver.raw;
		}
	} bootstrap;
}
