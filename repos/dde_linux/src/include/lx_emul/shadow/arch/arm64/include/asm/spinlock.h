/*
 * \brief  Shadows Linux kernel arch/arm/include/asm/spinlock.h
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

#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <linux/prefetch.h>
#include <asm/barrier.h>
#include <asm/processor.h>

#include <lx_emul/debug.h>


static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	return (atomic_read(&lock->val)) ? 1 : 0;
}


static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	if (arch_spin_is_locked(lock)) {
		printk("Error: spinlock contention!");
		lx_emul_trace_and_stop(__func__);
	}
	atomic_set(&lock->val, 1);
}


static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	atomic_set(&lock->val, 0);
}


static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	if (arch_spin_is_locked(lock))
		return 0;

	arch_spin_lock(lock);
	return 1;
}


static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	if (rw->wlocked)
		return 0;

	rw->wlocked = 1;
	return 1;
}


static inline void arch_write_lock(arch_rwlock_t *rw)
{
	if (rw->wlocked) {
		printk("Error: rwlock contention!");
		lx_emul_trace_and_stop(__func__);
	}
	rw->wlocked = 1;
}


static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	rw->wlocked = 0;
}


static inline void arch_read_lock(arch_rwlock_t *rw)
{
	arch_write_lock(rw);
}


static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	arch_write_unlock(rw);
}


static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	return arch_write_trylock(rw);
}


#endif /* __ASM_SPINLOCK_H */
