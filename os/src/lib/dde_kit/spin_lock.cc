/*
 * \brief   Spin lock
 * \author  Norman Feske
 * \date    2012-01-27
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <cpu/atomic.h>
#include <base/printf.h>

#include <spin_lock.h>

extern "C" {
#include <dde_kit/spin_lock.h>
}


extern "C" void dde_kit_spin_lock_init(dde_kit_spin_lock *spin_lock)
{
	*spin_lock = SPINLOCK_UNLOCKED;
}


extern "C" void dde_kit_spin_lock_lock(dde_kit_spin_lock *spin_lock)
{
	spinlock_lock(spin_lock);
}


extern "C" int dde_kit_spin_lock_try_lock(dde_kit_spin_lock *spin_lock)
{
	PERR("not implemented - will potentially block");

	spinlock_lock(spin_lock);

	/* success */
	return 0;
}


extern "C" void dde_kit_spin_lock_unlock(dde_kit_spin_lock *spin_lock)
{
	spinlock_unlock(spin_lock);
}

