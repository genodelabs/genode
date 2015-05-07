
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

/* seL4 includes */
#include <sel4/interfaces/sel4_client.h>


static inline void thread_yield() { seL4_Yield(); }
