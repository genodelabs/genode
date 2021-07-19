/*
 * \brief  Replaces kernel/locking/spinlock.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 *
 * We run single-core, cooperatively scheduled. We should never spin.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/spinlock.h>
#include <linux/rwlock_api_smp.h>

#include <lx_emul/debug.h>
#include <lx_emul/task.h>

void __lockfunc _raw_spin_lock(raw_spinlock_t * lock)
{
	if (atomic_read(&lock->raw_lock.val)) {
		printk("Error: spinlock contention!");
		lx_emul_trace_and_stop(__func__);
	}
	atomic_set(&lock->raw_lock.val, 1);
}


unsigned long __lockfunc _raw_spin_lock_irqsave(raw_spinlock_t * lock)
{
	unsigned long flags;
	local_irq_save(flags);
	_raw_spin_lock(lock);
	return flags;
}


void __lockfunc _raw_spin_unlock(raw_spinlock_t * lock)
{
	atomic_set(&lock->raw_lock.val, 0);
}


void __lockfunc _raw_spin_unlock_irqrestore(raw_spinlock_t * lock,
                                            unsigned long flags)
{
	_raw_spin_unlock(lock);
	local_irq_restore(flags);
}


void __lockfunc _raw_spin_lock_irq(raw_spinlock_t * lock)
{
	_raw_spin_lock_irqsave(lock);
}


void __lockfunc _raw_spin_unlock_irq(raw_spinlock_t * lock)
{
	_raw_spin_unlock_irqrestore(lock, 0);
}


int __lockfunc _raw_spin_trylock(raw_spinlock_t * lock)
{

	if (atomic_read(&lock->raw_lock.val))
		return 0;

	_raw_spin_lock(lock);
	return 1;
}


void __lockfunc _raw_write_lock(rwlock_t * lock)
{
	if (lock->raw_lock.wlocked) {
		printk("Error: rwlock contention!");
		lx_emul_trace_and_stop(__func__);
	}
	lock->raw_lock.wlocked = 1;
}


void __lockfunc _raw_write_unlock(rwlock_t * lock)
{
	lock->raw_lock.wlocked = 0;
}
