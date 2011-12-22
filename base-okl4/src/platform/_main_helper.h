/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Christian Prochaska
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM___MAIN_HELPER_H_
#define _PLATFORM___MAIN_HELPER_H_


/* OKL4-specific includes and definitions */
namespace Okl4 { extern "C" {
#include <l4/utcb.h>
#include <l4/thread.h>
} }

enum { L4_PAGEMASK = ~0xFFF };
enum { L4_PAGESIZE = 0x1000 };

namespace Okl4 {

	/*
	 * Read global thread ID from user-defined handle and store it
	 * into a designated UTCB entry.
	 */
	void copy_uregister_to_utcb()
	{
		using namespace Okl4;

		L4_Word_t my_global_id = L4_UserDefinedHandle();
		__L4_TCR_Set_ThreadWord(Genode::UTCB_TCR_THREAD_WORD_MYSELF,
		                        my_global_id);
	}
}


static void main_thread_bootstrap()
{
	/* copy thread ID to utcb */
	Okl4::copy_uregister_to_utcb();
}

#endif /* _PLATFORM___MAIN_HELPER_H_ */
