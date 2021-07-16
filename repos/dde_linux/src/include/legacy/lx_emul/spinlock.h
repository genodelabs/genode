/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/**********************
 ** linux/spinlock.h **
 **********************/

typedef struct spinlock { unsigned unused; } spinlock_t;
typedef struct raw_spinlock { unsigned dummy; } raw_spinlock_t;
#define DEFINE_SPINLOCK(name) spinlock_t name

void spin_lock(spinlock_t *lock);
void spin_lock_nested(spinlock_t *lock, int subclass);
void spin_lock_irqsave_nested(spinlock_t *lock, unsigned flags, int subclass);
void spin_unlock(spinlock_t *lock);
void spin_lock_init(spinlock_t *lock);
void spin_lock_irqsave(spinlock_t *lock, unsigned long flags);
void spin_lock_irqrestore(spinlock_t *lock, unsigned long flags);
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);
void spin_lock_irq(spinlock_t *lock);
void spin_unlock_irq(spinlock_t *lock);
void assert_spin_locked(spinlock_t *lock);
void spin_lock_bh(spinlock_t *lock);
void spin_unlock_bh(spinlock_t *lock);
int spin_trylock(spinlock_t *lock);

void raw_spin_lock_init(raw_spinlock_t *);
#define __RAW_SPIN_LOCK_UNLOCKED(raw_spinlock_t) {0}

/****************************
 ** linux/spinlock_types.h **
 ****************************/

#define __SPIN_LOCK_UNLOCKED(x) {0}
