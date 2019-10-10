/*
 * \brief   Kernel lock
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__SMP__KERNEL__LOCK_H_
#define _CORE__SPEC__SMP__KERNEL__LOCK_H_

namespace Kernel
{
	class Lock;

	Lock & data_lock();
}


class Kernel::Lock
{
	private:

		enum { INVALID = ~0U };

		enum State { UNLOCKED, LOCKED };

		int volatile      _locked      { UNLOCKED };
		unsigned volatile _current_cpu { INVALID  };

	public:

		void lock();
		void unlock();

		using Guard = Genode::Lock_guard<Lock>;
};

#endif /* _CORE__SPEC__SMP__KERNEL__LOCK_H_ */
