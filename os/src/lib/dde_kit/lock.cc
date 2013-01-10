/*
 * \brief   Locks (i.e., mutex)
 * \author  Christian Helmuth
 * \date    2008-08-15
 *
 * Open issues:
 *
 * - What about slabs for locks?
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/lock.h>
#include <base/printf.h>

extern "C" {
#include <dde_kit/lock.h>
}

struct dde_kit_lock { };


extern "C" void dde_kit_lock_init(struct dde_kit_lock **out_lock)
{
	try {
		*out_lock = reinterpret_cast<struct dde_kit_lock *>
		            (new (Genode::env()->heap()) Genode::Lock());
	} catch (...) {
		PERR("lock creation failed");
	}
}


extern "C" void dde_kit_lock_deinit(struct dde_kit_lock *lock)
{
	try {
		destroy(Genode::env()->heap(), reinterpret_cast<Genode::Lock *>(lock));
	} catch (...) { }
}


extern "C" void dde_kit_lock_lock(struct dde_kit_lock *lock)
{
	reinterpret_cast<Genode::Lock *>(lock)->lock();
}


extern "C" int dde_kit_lock_try_lock(struct dde_kit_lock *lock)
{
	PERR("not implemented - will potentially block");
	reinterpret_cast<Genode::Lock *>(lock)->lock();

	/* success */
	return 0;
}


extern "C" void dde_kit_lock_unlock(struct dde_kit_lock *lock)
{
	reinterpret_cast<Genode::Lock *>(lock)->unlock();
}
