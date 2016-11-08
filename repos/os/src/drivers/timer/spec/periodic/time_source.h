/*
 * \brief  Time source that uses sleeping by the means of the kernel
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIME_SOURCE_H_
#define _TIME_SOURCE_H_

/* local includes */
#include <threaded_time_source.h>

namespace Timer { class Time_source; }


class Timer::Time_source : public Threaded_time_source
{
	private:

		Genode::Lock mutable _lock;
		unsigned long        _curr_time_us = 0;
		unsigned long        _next_timeout_us = max_timeout().value;

		void _usleep(unsigned long us);


		/**************************
		 ** Threaded_time_source **
		 **************************/

		void _wait_for_irq();

	public:

		Time_source(Genode::Entrypoint &ep) : Threaded_time_source(ep) {
			start(); }


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Microseconds curr_time() const override;
		Microseconds max_timeout() const override;
		void schedule_timeout(Microseconds duration, Timeout_handler &handler) override;
};

#endif /* _TIME_SOURCE_H_ */
