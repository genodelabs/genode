/*
 * \brief  Mutex primitives
 * \author Alexander Boettcher
 * \date   2020-01-24
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__MUTEX_H_
#define _INCLUDE__BASE__MUTEX_H_

#include <base/lock.h>
#include <util/noncopyable.h>

namespace Genode { class Mutex; }


class Genode::Mutex : Noncopyable
{
	private:
		Lock _lock { };

	public:
		explicit Mutex() { }

		void acquire();
		void release();

		class Guard
		{
			private:
				Mutex &_mutex;

			public:
				explicit Guard(Mutex &mutex) : _mutex(mutex) { _mutex.acquire(); }

				~Guard() { _mutex.release(); }
		};
};

#endif /* _INCLUDE__BASE__MUTEX_H_ */
