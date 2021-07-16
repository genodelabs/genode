/*
 * \brief  Implementation of linux/spinlock.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

void spin_lock_init(spinlock_t *lock) { TRACE; }
void spin_lock_irqsave(spinlock_t *lock, unsigned long flags) { }
void spin_unlock(spinlock_t *lock) { TRACE; }
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags) { }
