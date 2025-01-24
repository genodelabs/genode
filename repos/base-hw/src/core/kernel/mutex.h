/*
 * \brief   Kernel mutex
 * \author  Stefan Kalkowski
 * \date    2024-11-22
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__MUTEX_H_
#define _CORE__KERNEL__MUTEX_H_

namespace Kernel { class Mutex; }


class Kernel::Mutex
{
	private:

		enum { INVALID = ~0U };

		enum State { UNLOCKED, LOCKED };

		State volatile    _locked      { UNLOCKED };
		unsigned volatile _current_cpu { INVALID  };

		bool _lock();
		void _unlock();

	public:

		/**
		 * Execute exclusively some critical section with lambda 'fn',
		 * if the critical section is called recursively by the same cpu
		 * lambda 'reentered' is called instead.
		 */
		void execute_exclusive(auto const &fn,
		                       auto const &reentered)
		{
			/*
			 * If the lock cannot get acquired, it is already taken by this cpu.
			 * That means implicitely that most probably some machine exception
			 * during kernel execution forced the cpu to re-enter this critical
			 * section.
			 */
			if (!_lock()) {
				reentered();

				/* block forever */
				while (!_lock()) ;
			}

			fn();
			_unlock();
		}
};

#endif /* _CORE__KERNEL__MUTEX_H_ */
