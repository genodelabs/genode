/*
 * \brief  Spin-lock implementation for environment's capability -allocator.
 * \author Stefan Kalkowski
 * \date   2012-02-29
 *
 * This is a Fiasco.OC-specific addition to the process enviroment.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/cap_map.h>
#include <base/internal/spin_lock.h>


Genode::Spin_lock::Spin_lock() : _spinlock(SPINLOCK_UNLOCKED) {}


void Genode::Spin_lock::lock()   { spinlock_lock(&_spinlock);   }


void Genode::Spin_lock::unlock() { spinlock_unlock(&_spinlock); }
