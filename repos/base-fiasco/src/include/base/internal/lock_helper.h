/*
 * \brief  L4/Fiasco-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2012-03-01
 *
 * L4/Fiasco is the only kernel that does not rely on Genode's generic lock
 * implementation. The custom implementation contained in 'lock.cc' does not
 * need 'lock_helper.h'. This file exists for the sole reason to make the
 * L4/Fiasco version of 'lock_helper' usable from the DDE Kit's spin lock.
 * Otherwise, we would need to add a special case for L4/Fiasco to the DDE Kit
 * library.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

/* L4/Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
}


/**
 * Yield CPU time
 */
static inline void thread_yield()
{
	Fiasco::l4_ipc_sleep(Fiasco::l4_ipc_timeout(0, 0, 500, 0));
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
