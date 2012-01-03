/*
 * \brief  Basic locking primitive
 * \author Norman Feske
 * \date   2006-07-26
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CANCELABLE_LOCK_H_
#define _INCLUDE__BASE__CANCELABLE_LOCK_H_

#include <base/lock_guard.h>
#include <base/native_types.h>
#include <base/blocking.h>

namespace Genode {

	class Cancelable_lock
	{
		private:

			Native_lock _native_lock;

		public:

			enum State { LOCKED, UNLOCKED };

			/**
			 * Constructor
			 */
			Cancelable_lock(State initial = UNLOCKED);

			/**
			 * Try to aquire lock an block while lock is not free
			 *
			 * This function may throw a Genode::Blocking_canceled exception.
			 */
			void lock();

			/**
			 * Release lock
			 */
			void unlock();

			/**
			 * Lock guard
			 */
			typedef Genode::Lock_guard<Cancelable_lock> Guard;
	};
}

#endif /* _INCLUDE__BASE__CANCELABLE_LOCK_H_ */
