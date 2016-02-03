/*
 * \brief   Reentrant lock
 * \author  Norman Feske
 * \date    2015-05-04
 *
 * Generally, well-designed software should not require a reentrant lock.
 * However, the circular dependency between core's virtual address space and
 * the backing store needed for managing the meta data of core's page tables
 * and page table entries cannot easily be dissolved otherwise.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__REENTRANT_LOCK_H_
#define _CORE__INCLUDE__REENTRANT_LOCK_H_

/* Genode includes */
#include <base/lock.h>

namespace Genode { struct Reentrant_lock; }


struct Genode::Reentrant_lock
{
	Lock _lock;

	struct Guard : List<Guard>::Element, Noncopyable
	{
		Reentrant_lock &reentrant_lock;

		Thread_base const * const myself = Thread_base::myself();

		Guard(Reentrant_lock &reentrant_lock)
		:
			reentrant_lock(reentrant_lock)
		{
			/*
			 * Don't do anything if we are in a nested call
			 */
			if (reentrant_lock._myself_already_registered())
				return;

			/*
			 * We are the top-level caller. Register ourself at
			 * the '_callers' list so that nested calls will be
			 * able to detect us (by calling '_top_level_caller'.
			 */
			{
				Lock::Guard guard(reentrant_lock._callers_lock);
				reentrant_lock._callers.insert(this);
			}

			/*
			 * Since we are the initial caller, we can safely take
			 * the lock without risking a deadlock.
			 */
			reentrant_lock._lock.lock();
		}

		~Guard()
		{
			if (!reentrant_lock._registered(this))
				return;

			Lock::Guard guard(reentrant_lock._callers_lock);
			reentrant_lock._callers.remove(this);

			reentrant_lock._lock.unlock();
		}
	};

	Lock        _callers_lock;
	List<Guard> _callers;

	bool _myself_already_registered()
	{
		Lock::Guard guard(_callers_lock);

		Thread_base const * const myself = Thread_base::myself();

		for (Guard *c = _callers.first(); c; c = c->next())
			if (c->myself == myself)
				return true;

		return false;
	}

	bool _registered(Guard const * const caller)
	{
		Lock::Guard guard(_callers_lock);

		for (Guard *c = _callers.first(); c; c = c->next())
			if (c == caller)
				return true;

		return false;
	}
};

#endif /* _CORE__INCLUDE__REENTRANT_LOCK_H_ */
