/*
 * \brief  Interface for accessing the libc's kernel timer
 * \author Norman Feske
 * \date   2019-09-19
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__KERNEL_TIMER_ACCESSOR_H_
#define _LIBC__INTERNAL__KERNEL_TIMER_ACCESSOR_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <timer_session/connection.h>

namespace Libc { struct Kernel_timer_accessor; }

struct Libc::Kernel_timer_accessor : Timer_accessor
{
	Genode::Env &_env;

	/*
	 * The '_timer' is constructed by whatever thread (main thread
	 * of pthread) that uses a time-related function first. Hence,
	 * the construction must be protected by a lock.
	 */
	Lock _lock;

	Constructible<Timer> _timer;

	Kernel_timer_accessor(Genode::Env &env) : _env(env) { }

	Timer &timer() override
	{
		Lock::Guard guard(_lock);

		if (!_timer.constructed())
			_timer.construct(_env);

		return *_timer;
	}
};

#endif /* _LIBC__INTERNAL__KERNEL_TIMER_ACCESSOR_H_ */
