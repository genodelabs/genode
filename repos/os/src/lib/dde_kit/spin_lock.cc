/*
 * \brief   Spin lock
 * \author  Norman Feske
 * \date    2012-01-27
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <cpu/atomic.h>
#include <base/printf.h>

#include <spin_lock.h> /* included from 'base/src/base/lock/' */

extern "C" {
#include <dde_kit/spin_lock.h>
}


template <bool b> struct Static_assert { };

/* specialization for successful static assertion */
template <> struct Static_assert<true> { static void assert() { } };


extern "C" void dde_kit_spin_lock_init(dde_kit_spin_lock *spin_lock)
{
	/*
	 * Check at compile time that the enum values defined for DDE Kit
	 * correspond to those defined in 'base/src/base/lock/spin_lock.h'.
	 */
	Static_assert<(int)DDE_KIT_SPIN_LOCK_LOCKED   == (int)SPINLOCK_LOCKED  >::assert();
	Static_assert<(int)DDE_KIT_SPIN_LOCK_UNLOCKED == (int)SPINLOCK_UNLOCKED>::assert();

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

