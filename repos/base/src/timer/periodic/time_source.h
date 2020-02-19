/*
 * \brief  Time source that uses sleeping by the means of the kernel
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIME_SOURCE_H_
#define _TIME_SOURCE_H_

/* local includes */
#include <threaded_time_source.h>

namespace Timer {

	using Genode::uint64_t;
	class Time_source;
}


class Timer::Time_source : public Threaded_time_source
{
	private:

		Genode::Env         &_env;

		Genode::Mutex mutable _mutex { };
		uint64_t              _curr_time_us = 0;
		uint64_t              _next_timeout_us = max_timeout().value;

		void _usleep(uint64_t us);


		/**************************
		 ** Threaded_time_source **
		 **************************/

		void _wait_for_irq() override;

	public:

		Time_source(Genode::Env &env)
		: Threaded_time_source(env), _env(env) { start(); }


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Duration curr_time() override;
		Microseconds max_timeout() const override;
		void schedule_timeout(Microseconds duration, Timeout_handler &handler) override;
};

#endif /* _TIME_SOURCE_H_ */
