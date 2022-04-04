/*
 * \brief  Architecture-specific accessors to spinlock types for lx_emul
 * \author Johannes Schlatow
 * \date   2022-04-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__ARCH_SPINLOCK_H_
#define _LX_EMUL__ARCH_SPINLOCK_H_

#define SPINLOCK_VALUE_PTR(lock) (atomic_t*)&lock->raw_lock.slock
#define RWLOCK_VALUE(lock)                   lock->raw_lock.lock

#endif /*_LX_EMUL__ARCH_SPINLOCK_H_ */
