/*
 * \brief  A wrapper for Timer::Connection that caches time values
 * \author Johannes Schlatow
 * \date   2022-07-07
 *
 * This implementation aims for reducing the number of
 * Timer::Connection::curr_time() that was found to be relatively
 * expensive on base-hw (implies a syscall on each call) by assuming that
 * a certain caching is fine with the accuracy requirements of the NIC router.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CACHED_TIMER_H_
#define _CACHED_TIMER_H_

/* Genode includes */
#include <timer_session/connection.h>

namespace Net {

	class Cached_timer;
}


class Net::Cached_timer : public ::Timer::Connection
{
	private:

		using Duration     = Genode::Duration;
		using Microseconds = Genode::Microseconds;

		Duration _cached_time { Microseconds { 0 } };

	public:

		Cached_timer (Genode::Env &env)
		:
			Timer::Connection { env }
		{ }

		/**
		 * Update cached time with current timer
		 */
		void update_cached_time()
		{
			_cached_time = Timer::Connection::curr_time();
		}

		/**
		 * Update cached time and return it
		 */
		Duration curr_time() override
		{
			update_cached_time();
			return cached_time();
		}


		/***************
		 ** Accessors **
		 ***************/

		Duration cached_time() const { return _cached_time; }

		void cached_time(Duration time) { _cached_time = time; }
};

#endif /* _CACHED_TIMER_H_ */
