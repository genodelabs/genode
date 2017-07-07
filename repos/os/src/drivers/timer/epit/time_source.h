/*
 * \brief  Time source that uses the Enhanced Periodic Interrupt Timer (Freescale)
 * \author Norman Feske
 * \author Martin Stein
 * \author Stefan Kalkowski
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

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <irq_session/connection.h>
#include <os/duration.h>

/* local includes */
#include <signalled_time_source.h>

#include "epit.h"

namespace Timer {

	using Microseconds = Genode::Microseconds;
	using Duration     = Genode::Duration;
	class Time_source;
}


class Timer::Time_source : public Genode::Signalled_time_source
{
	private:

		Genode::Attached_io_mem_dataspace _io_mem;
		Genode::Irq_connection            _timer_irq;
		Genode::Epit_base                 _epit;
		unsigned long long mutable        _curr_time_us { 0 };

	public:

		Time_source(Genode::Env &env);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Duration curr_time() override;
		void schedule_timeout(Microseconds duration, Timeout_handler &handler) override;
		Microseconds max_timeout() const override;
};

#endif /* _TIME_SOURCE_H_ */
