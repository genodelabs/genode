/*
 * \brief  Blockade primitives
 * \author Alexander Boettcher
 * \date   2020-01-24
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
		explicit Blockade() : _lock(Lock::LOCKED) { }

		void block() { _lock.lock(); }

		void wakeup() { _lock.unlock(); }
};

#endif /* _INCLUDE__BASE__BLOCKADE_H_ */
