/*
 * \brief  Basic locking primitive
 * \author Norman Feske
 * \date   2006-07-26
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
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

			class Applicant
			{
				private:

					Native_thread_id _tid;
					Applicant       *_to_wake_up;

				public:

					explicit Applicant(Native_thread_id tid)
					: _tid(tid), _to_wake_up(0) { }

					void applicant_to_wake_up(Applicant *to_wake_up) {
						_to_wake_up = to_wake_up; }

					Applicant *applicant_to_wake_up() { return _to_wake_up; }

					Native_thread_id tid() { return _tid; }

					/**
					 * Called from previous lock owner
					 */
					void wake_up();

					bool operator == (Applicant &a) { return _tid == a.tid(); }
					bool operator != (Applicant &a) { return _tid != a.tid(); }
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
