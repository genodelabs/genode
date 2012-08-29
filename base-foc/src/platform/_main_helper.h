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

	Cap_index *i
		= cap_map()->insert(MAIN_THREAD_CAP_ID, Fiasco::MAIN_THREAD_CAP);
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_BADGE] = (unsigned long) i;
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_THREAD_OBJ] = 0;
}


#endif /* _PLATFORM___MAIN_HELPER_H_ */
