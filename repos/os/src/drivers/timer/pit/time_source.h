/*
 * \brief  Time source that uses the Programmable Interval Timer (PIT)
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

/* Genode includes */
#include <io_port_session/connection.h>
#include <irq_session/connection.h>
#include <os/duration.h>

/* local includes */
#include <signalled_time_source.h>

namespace Timer {

	using Microseconds = Genode::Microseconds;
	using Duration     = Genode::Duration;
	class Time_source;
}


class Timer::Time_source : public Genode::Signalled_time_source
{
	private:


		enum {
			PIT_TICKS_PER_SECOND = 1193182,
			PIT_TICKS_PER_MSEC   = PIT_TICKS_PER_SECOND/1000,
			PIT_MAX_COUNT        =   65535,
			PIT_DATA_PORT_0      =    0x40,  /* data port for PIT channel 0,
			                                    connected to the PIC */
			PIT_CMD_PORT         =    0x43,  /* PIT command port */

			PIT_MAX_USEC = (PIT_MAX_COUNT*1000)/(PIT_TICKS_PER_MSEC),

			IRQ_PIT = 0,  /* timer interrupt at the PIC */

			/*
			 * Bit definitions for accessing the PIT command port
			 */
			PIT_CMD_SELECT_CHANNEL_0 = 0 << 6,
			PIT_CMD_ACCESS_LO        = 1 << 4,
			PIT_CMD_ACCESS_LO_HI     = 3 << 4,
			PIT_CMD_MODE_IRQ         = 0 << 1,
			PIT_CMD_MODE_RATE        = 2 << 1,

			PIT_CMD_READ_BACK        = 3 << 6,
			PIT_CMD_RB_COUNT         = 0 << 5,
			PIT_CMD_RB_STATUS        = 0 << 4,
			PIT_CMD_RB_CHANNEL_0     = 1 << 1,

			/*
			 * Bit definitions of the PIT status byte
			 */
			PIT_STAT_INT_LINE = 1 << 7,
		};

		Genode::Io_port_connection _io_port;
		Genode::Irq_connection     _timer_irq;
		unsigned long    mutable   _curr_time_us = 0;
		Genode::uint16_t mutable   _counter_init_value = 0;
		bool             mutable   _handled_wrap = false;

		void _set_counter(Genode::uint16_t value);

		Genode::uint16_t _read_counter(bool *wrapped);

		Genode::uint32_t _ticks_since_update_one_wrap(Genode::uint16_t curr_counter);

		Genode::uint32_t _ticks_since_update_no_wrap(Genode::uint16_t curr_counter);

		Duration _curr_time();

	public:

		Time_source(Genode::Env &env);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Duration curr_time() override;
		void schedule_timeout(Microseconds duration, Timeout_handler &handler) override;
		Microseconds max_timeout() const override {
			return Microseconds(PIT_MAX_USEC); }
};

#endif /* _TIME_SOURCE_H_ */
