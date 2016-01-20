/*
 * \brief  Implementation of platform-specific capabilities
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/capability.h>

/* base-internal includes */
#include <base/internal/spin_lock.h>

static volatile int    spinlock = SPINLOCK_UNLOCKED;
static Genode::uint8_t ref_counter[1 << (sizeof(Kernel::capid_t)*8)];

void Genode::Native_capability::_inc() const
{
	if (!valid()) return;

	spinlock_lock(&spinlock);
	ref_counter[_dst]++;
	spinlock_unlock(&spinlock);
}


void Genode::Native_capability::_dec() const
{
	if (!valid()) return;

	spinlock_lock(&spinlock);
	if (!--ref_counter[_dst]) { Kernel::delete_cap(_dst); }
	spinlock_unlock(&spinlock);
}
