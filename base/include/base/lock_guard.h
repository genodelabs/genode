/*
 * \brief  Lock guard
 * \author Norman Feske
 * \date   2006-07-26
 *
 * A lock guard is instantiated as a local variable.
 * When a lock guard is constructed, it acquires the lock that
 * is specified as constructor argument. When the control
 * flow leaves the scope of the lock guard variable via
 * a return statement or an exception, the lock guard's
 * destructor gets called, freeing the lock.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__LOCK_GUARD_H_
#define _INCLUDE__BASE__LOCK_GUARD_H_

namespace Genode {

	/**
	 * Lock guard template
	 *
	 * \param LT  lock type
	 */
	template <typename LT>
	class Lock_guard
	{
		private:

			LT &_lock;

		public:

			explicit Lock_guard(LT &lock) : _lock(lock) { _lock.lock(); }

			~Lock_guard() { _lock.unlock(); }
	};
}

#endif /* _INCLUDE__BASE__LOCK_GUARD_H_ */
