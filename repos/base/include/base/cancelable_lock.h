/*
 * \brief  Basic locking primitive
 * \author Norman Feske
 * \date   2006-07-26
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__CANCELABLE_LOCK_H_
#define _INCLUDE__BASE__CANCELABLE_LOCK_H_

#include <base/lock_guard.h>
#include <base/blocking.h>

namespace Genode {

	class Thread;
	class Cancelable_lock;
}


class Genode::Cancelable_lock
{
	private:

		class Applicant
		{
			private:

				Thread    *_thread_base;
				Applicant *_to_wake_up;

			public:

				explicit Applicant(Thread *thread_base)
				: _thread_base(thread_base), _to_wake_up(0) { }

				void applicant_to_wake_up(Applicant *to_wake_up) {
					_to_wake_up = to_wake_up; }

				Applicant *applicant_to_wake_up() { return _to_wake_up; }

				Thread *thread_base() { return _thread_base; }

				/**
				 * Called from previous lock owner
				 */
				void wake_up();

				bool operator == (Applicant &a) { return _thread_base == a.thread_base(); }
				bool operator != (Applicant &a) { return _thread_base != a.thread_base(); }
		};

		/*
		 * Note that modifications of the applicants queue must be performed
		 * atomically. Hence, we use the additional spinlock here.
		 */

		volatile int _spinlock_state;
		volatile int _state;

		Applicant* volatile _last_applicant;
		Applicant  _owner;

	public:

		enum State { LOCKED, UNLOCKED };

		/**
		 * Constructor
		 */
		explicit Cancelable_lock(State initial = UNLOCKED);

		/**
		 * Try to aquire the lock and block while the lock is not free
		 *
		 * \throw  Genode::Blocking_canceled
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

#endif /* _INCLUDE__BASE__CANCELABLE_LOCK_H_ */
