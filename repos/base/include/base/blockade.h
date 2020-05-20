/*
 * \brief  Blockade primitive
 * \author Alexander Boettcher
 * \date   2020-01-24
 *
 * A 'Blockade' is a locking primitive designated for deliberately pausing
 * the execution of a thread until woken up by another thread. It is typically
 * used as a mechanism for synchonizing the startup of threads.
 *
 * At initialization time, a blockade is in locked state. A thread can pause
 * its own execution by calling the 'block()' method. Another thread can wake
 * up the paused thread by calling 'wakeup()' on the blockade.
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__BLOCKADE_H_
#define _INCLUDE__BASE__BLOCKADE_H_

#include <base/lock.h>
#include <util/noncopyable.h>

namespace Genode { class Blockade; }


class Genode::Blockade : Noncopyable
{
	private:

		Lock _lock;

	public:

		Blockade() : _lock(Lock::LOCKED) { }

		void block() { _lock.lock(); }

		void wakeup() { _lock.unlock(); }
};

#endif /* _INCLUDE__BASE__BLOCKADE_H_ */
