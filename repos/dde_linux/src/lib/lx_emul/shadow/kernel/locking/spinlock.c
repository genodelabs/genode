/*
 * \brief  Replaces kernel/locking/spinlock.c
 * \author Stefan Kalkowski
 * \author Johannes Schlatow
 * \author Christian Helmuth
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

#ifdef CONFIG_SMP

#include <linux/rwlock_api_smp.h>
#include <asm/spinlock.h>

#include <lx_emul/debug.h>
#include <lx_emul/task.h>


#ifndef CONFIG_INLINE_SPIN_LOCK
void __lockfunc _raw_spin_lock(raw_spinlock_t * lock)
{
	arch_spin_lock(&lock->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_SPIN_LOCK_BH
void __lockfunc _raw_spin_lock_bh(raw_spinlock_t * lock)
{
	__local_bh_disable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
	_raw_spin_lock(lock);
}
#endif


#ifndef CONFIG_INLINE_SPIN_LOCK_IRQ
void __lockfunc _raw_spin_lock_irq(raw_spinlock_t * lock)
{
	_raw_spin_lock_irqsave(lock);
}
#endif


#ifndef CONFIG_INLINE_SPIN_LOCK_IRQSAVE
unsigned long __lockfunc _raw_spin_lock_irqsave(raw_spinlock_t * lock)
{
	unsigned long flags;
	local_irq_save(flags);
	_raw_spin_lock(lock);
	return flags;
}
#endif


#ifndef CONFIG_INLINE_SPIN_TRYLOCK
int __lockfunc _raw_spin_trylock(raw_spinlock_t * lock)
{
	return arch_spin_trylock(&lock->raw_lock);
}
#endif


#ifdef CONFIG_UNINLINE_SPIN_UNLOCK
void __lockfunc _raw_spin_unlock(raw_spinlock_t * lock)
{
	arch_spin_unlock(&lock->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_SPIN_UNLOCK_BH
void __lockfunc _raw_spin_unlock_bh(raw_spinlock_t * lock)
{
	_raw_spin_unlock(lock);
	__local_bh_enable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
}
#endif


#ifndef CONFIG_INLINE_SPIN_UNLOCK_IRQ
void __lockfunc _raw_spin_unlock_irq(raw_spinlock_t * lock)
{
	_raw_spin_unlock_irqrestore(lock, 0);
}
#endif


#ifndef CONFIG_INLINE_SPIN_UNLOCK_IRQRESTORE
void __lockfunc _raw_spin_unlock_irqrestore(raw_spinlock_t * lock,
                                            unsigned long flags)
{
	_raw_spin_unlock(lock);
	local_irq_restore(flags);
}
#endif


#ifndef CONFIG_INLINE_READ_LOCK
void __lockfunc _raw_read_lock(rwlock_t * lock)
{
	arch_read_lock(&(lock)->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_READ_UNLOCK
void __lockfunc _raw_read_unlock(rwlock_t * lock)
{
	arch_read_unlock(&(lock)->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_READ_LOCK_IRQSAVE
unsigned long __lockfunc _raw_read_lock_irqsave(rwlock_t * lock)
{
	unsigned long flags;
	local_irq_save(flags);
	arch_read_lock(&(lock)->raw_lock);
	return flags;
}
#endif


#ifndef CONFIG_INLINE_WRITE_UNLOCK_BH
void __lockfunc _raw_write_unlock_bh(rwlock_t * lock)
{
	arch_write_unlock(&(lock)->raw_lock);
	__local_bh_enable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
}
#endif


#ifndef CONFIG_INLINE_READ_UNLOCK_BH
void __lockfunc _raw_read_unlock_bh(rwlock_t * lock)
{
	arch_read_unlock(&(lock)->raw_lock);
	__local_bh_enable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
}
#endif


#ifndef CONFIG_INLINE_WRITE_LOCK
void __lockfunc _raw_write_lock(rwlock_t * lock)
{
	arch_write_lock(&(lock)->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_WRITE_UNLOCK
void __lockfunc _raw_write_unlock(rwlock_t * lock)
{
	arch_write_unlock(&(lock)->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_READ_UNLOCK_IRQRESTORE
void __lockfunc _raw_read_unlock_irqrestore(rwlock_t * lock,unsigned long flags)
{
	arch_read_unlock(&(lock)->raw_lock);
	local_irq_restore(flags);
}
#endif


#ifndef CONFIG_INLINE_WRITE_LOCK_BH
void __lockfunc _raw_write_lock_bh(rwlock_t * lock)
{
	__local_bh_disable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
	arch_write_lock(&(lock)->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_WRITE_LOCK_IRQ
void __lockfunc _raw_write_lock_irq(rwlock_t * lock)
{
	arch_write_lock(&(lock)->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_READ_LOCK_BH
void __lockfunc _raw_read_lock_bh(rwlock_t * lock)
{
	__local_bh_disable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
	arch_read_lock(&(lock)->raw_lock);
}
#endif


#ifndef CONFIG_INLINE_WRITE_LOCK_IRQSAVE
unsigned long __lockfunc _raw_write_lock_irqsave(rwlock_t *lock)
{
	unsigned long flags;
	local_irq_save(flags);
	arch_write_lock(&(lock)->raw_lock);
	return flags;
}
#endif


#ifndef CONFIG_INLINE_WRITE_UNLOCK_IRQRESTORE
void __lockfunc _raw_write_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
{
	arch_write_unlock(&(lock)->raw_lock);
	local_irq_restore(flags);
}
#endif

#endif /* CONFIG_SMP */
