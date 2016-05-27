/*
 * \brief  Platform timer based on the Programmable Interval Timer (PIT)
 * \author Norman Feske
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_TIMER_H_
#define _PLATFORM_TIMER_H_

/* Genode includes */
#include <io_port_session/connection.h>
#include <irq_session/connection.h>
#include <os/server.h>

class Platform_timer
{
	private:

		enum {
			PIT_TICKS_PER_SECOND = 1193182,
			PIT_TICKS_PER_MSEC   = PIT_TICKS_PER_SECOND/1000,
			PIT_MAX_COUNT        =   65535,
			PIT_DATA_PORT_0      =    0x40,  /* data port for PIT channel 0,
                                                connected to the PIC */
			PIT_CMD_PORT         =    0x43,  /* PIT command port */

			PIT_MAX_USEC = (PIT_MAX_COUNT*1000)/(PIT_TICKS_PER_MSEC)
		};

		enum {
			IRQ_PIT = 0,  /* timer interrupt at the PIC */
		};

		/**
		 * Bit definitions for accessing the PIT command port
		 */
		enum {
			PIT_CMD_SELECT_CHANNEL_0 = 0 << 6,
			PIT_CMD_ACCESS_LO        = 1 << 4,
			PIT_CMD_ACCESS_LO_HI     = 3 << 4,
			PIT_CMD_MODE_IRQ         = 0 << 1,
			PIT_CMD_MODE_RATE        = 2 << 1,

			PIT_CMD_READ_BACK        = 3 << 6,
			PIT_CMD_RB_COUNT         = 0 << 5,
			PIT_CMD_RB_STATUS        = 0 << 4,
			PIT_CMD_RB_CHANNEL_0     = 1 << 1,
		};

		/**
		 * Bit definitions of the PIT status byte
		 */
		enum {
			PIT_STAT_INT_LINE = 1 << 7,
		};

		Genode::Io_port_connection _io_port;
		Genode::Irq_connection     _timer_irq;
		unsigned long mutable      _curr_time_usec;
		Genode::uint16_t mutable   _counter_init_value;
		bool          mutable      _handled_wrap;
		Genode::Signal_receiver    _irq_rec;
		Genode::Signal_context     _irq_ctx;

		/**
		 * Set PIT counter value
		 */
		void _set_counter(Genode::uint16_t value)
		{
			_handled_wrap = false;
			_io_port.outb(PIT_DATA_PORT_0, value & 0xff);
			_io_port.outb(PIT_DATA_PORT_0, (value >> 8) & 0xff);
		}

		/**
		 * Read current PIT counter value
		 */
		Genode::uint16_t _read_counter(bool *wrapped)
		{
			/* read-back count and status of counter 0 */
			_io_port.outb(PIT_CMD_PORT, PIT_CMD_READ_BACK |
			              PIT_CMD_RB_COUNT | PIT_CMD_RB_STATUS | PIT_CMD_RB_CHANNEL_0);

			/* read status byte from latch register */
			Genode::uint8_t status = _io_port.inb(PIT_DATA_PORT_0);

			/* read low and high bytes from latch register */
			Genode::uint16_t lo = _io_port.inb(PIT_DATA_PORT_0);
			Genode::uint16_t hi = _io_port.inb(PIT_DATA_PORT_0);

			*wrapped = status & PIT_STAT_INT_LINE ? true : false;
			return (hi << 8) | lo;
		}

	public:

		/**
		 * Constructor
		 */
		Platform_timer()
		:
			_io_port(PIT_DATA_PORT_0, PIT_CMD_PORT - PIT_DATA_PORT_0 + 1),
			_timer_irq(IRQ_PIT),
			_curr_time_usec(0),
			_counter_init_value(0),
			_handled_wrap(false)
		{
			/* operate PIT in one-shot mode */
			_io_port.outb(PIT_CMD_PORT, PIT_CMD_SELECT_CHANNEL_0 |
			              PIT_CMD_ACCESS_LO_HI | PIT_CMD_MODE_IRQ);

			_timer_irq.sigh(_irq_rec.manage(&_irq_ctx));
			_timer_irq.ack_irq();
		}

		~Platform_timer() { _irq_rec.dissolve(&_irq_ctx); }

		/**
		 * Return current time-counter value in microseconds
		 *
		 * This function has to be executed regularly,
		 * at least all max_timeout() usecs.
		 */
		unsigned long curr_time() const
		{
			Genode::uint32_t passed_ticks;

			/*
			 * Read PIT count and status
			 *
			 * Reading the PIT registers via port I/O is a non-const operation.
			 * Since 'curr_time' is declared as const, however, we need to
			 * explicitly override the const-ness of the 'this' pointer.
			 */
			bool wrapped;
			Genode::uint16_t const curr_counter = const_cast<Platform_timer *>(this)->_read_counter(&wrapped);

			/* determine the time since we looked at the counter */
			if (wrapped && !_handled_wrap) {
				passed_ticks = _counter_init_value;
				/* the counter really wrapped around */
				if (curr_counter)
					passed_ticks += PIT_MAX_COUNT + 1 - curr_counter;

				_handled_wrap = true;
			} else {
				if (_counter_init_value)
					passed_ticks = _counter_init_value - curr_counter;
				else
					passed_ticks = PIT_MAX_COUNT + 1 - curr_counter;
			}

			_curr_time_usec += (passed_ticks*1000)/PIT_TICKS_PER_MSEC;

			/* use current counter as the reference for the next update */
			_counter_init_value = curr_counter;

			return _curr_time_usec;
		}

		/**
		 * Return maximum timeout as supported by the platform
		 */
		unsigned long max_timeout() { return PIT_MAX_USEC; }

		/**
		 * Schedule next timeout
		 *
		 * \param timeout_usec  timeout in microseconds
		 *
		 * The maximum value for 'timeout_ms' is 54924 microseconds. If
		 * specifying a higher timeout, this maximum value will be scheduled.
		 */
		void schedule_timeout(unsigned long timeout_usec)
		{
			/* limit timer-interrupt rate */
			enum { MAX_TIMER_IRQS_PER_SECOND = 4*1000 };
			if (timeout_usec < 1000*1000/MAX_TIMER_IRQS_PER_SECOND)
				timeout_usec = 1000*1000/MAX_TIMER_IRQS_PER_SECOND;

			if (timeout_usec > max_timeout())
				timeout_usec = max_timeout();

			_counter_init_value = (PIT_TICKS_PER_MSEC * timeout_usec)/1000;
			_set_counter(_counter_init_value);
		}

		/**
		 * Block for the next scheduled timeout
		 */
		void wait_for_timeout(Genode::Thread *blocking_thread)
		{
			_irq_rec.wait_for_signal();
			_timer_irq.ack_irq();
		}
};

#endif /* _PLATFORM_TIMER_H_ */
