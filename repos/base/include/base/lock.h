/*
 * \brief  Basic locking primitive
 * \author Norman Feske
 * \date   2006-07-26
 */

/*
 * Copyright (C) 2006-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__LOCK_H_
#define _INCLUDE__BASE__LOCK_H_

namespace Genode {
	class Lock;
	class Thread;
	class Mutex;
}


class Genode::Lock
{
	friend class Blockade;
	friend class Mutex;

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

		volatile int _spinlock_state = 0;
		volatile int _state          = 0;

		Applicant * volatile _last_applicant = nullptr;

		Applicant _owner;

		bool lock_owner(Applicant &myself) {
			return (_state == LOCKED) && (_owner == myself); }

		void lock(Applicant &);

		enum State { LOCKED, UNLOCKED };

		/**
		 * Constructor
		 */
		explicit Lock(State);

		/**
		 * Try to acquire the lock and block while the lock is not free.
		 */
		void lock();

		/**
		 * Release lock
		 */
		void unlock();
};

#endif /* _INCLUDE__BASE__LOCK_H_ */
