/*
 * \brief  Locking primitives
 * \author Norman Feske
 * \date   2006-07-26
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__LOCK_H_
#define _INCLUDE__BASE__LOCK_H_

#include <base/cancelable_lock.h>

namespace Genode { class Lock; }


struct Genode::Lock : Cancelable_lock
{
	/**
	 * Constructor
	 */
	explicit Lock(State initial = UNLOCKED) : Cancelable_lock(initial) { }

	void lock()
	{
		while (1)
			try {
				Cancelable_lock::lock();
				return;
			} catch (Blocking_canceled) { }
	}

	/**
	 * Lock guard
	 */
	typedef Lock_guard<Lock> Guard;
};

#endif /* _INCLUDE__BASE__LOCK_H_ */
