/*
 * \brief   A timer dummy class for test-purposes
 * \author  Stefan Kalkowski
 * \date    2025-04-11
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__TIMER_H_
#define _CORE__KERNEL__TIMER_H_

#include <kernel/types.h>

namespace Kernel {
	class Timeout;
	struct Timer;
}


struct Kernel::Timeout
{
	virtual void timeout_triggered() {}
	virtual ~Timeout() { }
};


struct Kernel::Timer
{
	time_t   _time { 0 };
	Timeout *_timeout { nullptr };
	time_t   _next_timeout { 0 };

	time_t time() { return _time; }

	time_t schedule_timeout() { return 0; } /* dummy unneeded */

	void set_timeout(Timeout &timeout, time_t const duration)
	{
		_next_timeout = _time + duration;
		_timeout      = &timeout;
	}

	void add_time(time_t const duration)
	{
		_time += duration;
		if (_timeout && _next_timeout <= _time) {
			_timeout->timeout_triggered();
			_timeout = nullptr;
		}
	}
};

#endif
