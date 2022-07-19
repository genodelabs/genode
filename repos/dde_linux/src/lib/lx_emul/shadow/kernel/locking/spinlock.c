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
#include <asm/spinlock.h>

#include <lx_emul/debug.h>
#include <lx_emul/task.h>


void __lockfunc _raw_spin_lock(raw_spinlock_t * lock)
{
	arch_spin_lock(&lock->raw_lock);
}


unsigned long __lockfunc _raw_spin_lock_irqsave(raw_spinlock_t * lock)
{
	unsigned long flags;
	local_irq_save(flags);
	_raw_spin_lock(lock);
	return flags;
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


int __lockfunc _raw_spin_trylock(raw_spinlock_t * lock)
{
	return arch_spin_trylock(&lock->raw_lock);
}


void __lockfunc _raw_write_lock(rwlock_t * lock)
{
	arch_write_lock(&(lock)->raw_lock);
}


#ifndef CONFIG_X86
void __lockfunc __raw_spin_unlock(raw_spinlock_t * lock)
{
	arch_spin_unlock(&lock->raw_lock);
}


void __lockfunc _raw_spin_unlock_irq(raw_spinlock_t * lock)
{
	_raw_spin_unlock_irqrestore(lock, 0);
}


void __lockfunc _raw_write_unlock(rwlock_t * lock)
{
	arch_write_unlock(&(lock)->raw_lock);
}
#endif
