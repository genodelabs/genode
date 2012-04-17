/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Christian Prochaska
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM___MAIN_HELPER_H_
#define _PLATFORM___MAIN_HELPER_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/native_types.h>
#include <base/cap_map.h>

namespace Fiasco {
#include <l4/sys/utcb.h>
}

enum { MAIN_THREAD_CAP_ID = 1 };

static void main_thread_bootstrap() {
	using namespace Genode;

	/**
	 * Unfortunately ldso calls this function twice. So the second time when
	 * inserting the main thread's gate-capability an exception would be raised.
	 * At least on ARM we got problems when raising an exception that early,
	 * that's why we first check if the cap is already registered before
	 * inserting it.
	 */
	Cap_index *idx = cap_map()->find(MAIN_THREAD_CAP_ID);
	if (!idx) {
		Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_BADGE]      = MAIN_THREAD_CAP_ID;
		Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_THREAD_OBJ] = 0;
		cap_map()->insert(MAIN_THREAD_CAP_ID, Fiasco::MAIN_THREAD_CAP)->inc();
	}
}


#endif /* _PLATFORM___MAIN_HELPER_H_ */
