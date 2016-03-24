/*
 * \brief  seL4-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2015-05-07
 *
 * Based on the lock implementation of base-fiasco/src/base/lock/.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

/* seL4 includes */
#include <sel4/sel4.h>


static inline void thread_yield() { seL4_Yield(); }

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
