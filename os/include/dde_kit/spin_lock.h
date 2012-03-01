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

#ifndef _INCLUDE__DDE_KIT__SPIN_LOCK_H_
#define _INCLUDE__DDE_KIT__SPIN_LOCK_H_

/**
 * Private spin lock type
 */
typedef volatile int dde_kit_spin_lock;

enum { DDE_KIT_SPIN_LOCK_LOCKED, DDE_KIT_SPIN_LOCK_UNLOCKED };


/**
 * Initialize spin lock
 *
 * \param   out_lock  lock handle (output parameter)
 *
 * The created lock is free (not acquired) after initialization.
 */
void dde_kit_spin_lock_init(dde_kit_spin_lock *spin_lock);

/**
 * Acquire spin lock
 */
void dde_kit_spin_lock_lock(dde_kit_spin_lock *spin_lock);

/**
 * Try to acquire a lock (non-blocking)
 */
int dde_kit_spin_lock_try_lock(dde_kit_spin_lock *spin_lock);

/**
 * Release spin lock
 */
void dde_kit_spin_lock_unlock(dde_kit_spin_lock *spin_lock);


#endif /* _INCLUDE__DDE_KIT__SPIN_LOCK_H_ */
