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

namespace Fiasco {
#include <l4/sys/utcb.h>
}


static void main_thread_bootstrap() {
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_THREAD_OBJ] = 0; }


#endif /* _PLATFORM___MAIN_HELPER_H_ */
