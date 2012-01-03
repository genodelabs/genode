/*
 * \brief   Locks (i.e., mutex)
 * \author  Christian Helmuth
 * \date    2008-08-15
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__LOCK_H_
#define _INCLUDE__DDE_KIT__LOCK_H_

/**
 * Private lock type
 */
struct dde_kit_lock;

/**
 * Initialize lock
 *
 * \param   out_lock  lock handle (output parameter)
 *
 * The created lock is free (not acquired) after initialization.
 */
void dde_kit_lock_init(struct dde_kit_lock **out_lock);

/**
 * Destroy lock
 *
 * \param   lock  lock handle
 *
 * The old lock handle is invalidated.
 */
void dde_kit_lock_deinit(struct dde_kit_lock *lock);

/**
 * Acquire lock
 *
 * \param   lock  lock handle
 */
void dde_kit_lock_lock(struct dde_kit_lock *lock);

/**
 * Acquire a lock (non-blocking)
 *
 * \param   lock  lock handle
 *
 * \return  lock state
 *   \retval 0    lock was aquired
 *   \retval 1    lock was not aquired
 */
int dde_kit_lock_try_lock(struct dde_kit_lock *lock);

/**
 * Release lock
 *
 * \param   lock  lock handle
 */
void dde_kit_lock_unlock(struct dde_kit_lock *lock);

#endif /* _INCLUDE__DDE_KIT__LOCK_H_ */
