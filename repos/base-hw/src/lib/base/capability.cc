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

/* kernel includes */
#include <kernel/interface.h>

using namespace Genode;

static volatile int spinlock = SPINLOCK_UNLOCKED;

static uint8_t ref_counter[1 << (sizeof(Kernel::capid_t)*8)];


Native_capability::Native_capability() { }


void Native_capability::_inc()
{
	if (!valid()) return;

	spinlock_lock(&spinlock);
	ref_counter[(addr_t)_data]++;
	spinlock_unlock(&spinlock);
}


void Native_capability::_dec()
{
	if (!valid()) return;

	spinlock_lock(&spinlock);
	if (!--ref_counter[(addr_t)_data]) { Kernel::delete_cap((addr_t)_data); }
	spinlock_unlock(&spinlock);
}


long Native_capability::local_name() const
{
	return (long)_data;
}


bool Native_capability::valid() const
{
	return (addr_t)_data != Kernel::cap_id_invalid();
}


Native_capability::Raw Native_capability::raw() const { return { 0, 0, 0, 0 }; }


void Native_capability::print(Genode::Output &out) const
{
	using Genode::print;

	print(out, "cap<");
	if (_data) {
		print(out, (addr_t)_data);
	} else {
		print(out, "invalid");
	}
	print(out, ">");
}
