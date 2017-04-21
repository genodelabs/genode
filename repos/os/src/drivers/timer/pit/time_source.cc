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

/* local includes */
#include <time_source.h>

using namespace Genode;


void Timer::Time_source::_set_counter(uint16_t value)
{
	_handled_wrap = false;
	_io_port.outb(PIT_DATA_PORT_0, value & 0xff);
	_io_port.outb(PIT_DATA_PORT_0, (value >> 8) & 0xff);
}


uint16_t Timer::Time_source::_read_counter(bool *wrapped)
{
	/* read-back count and status of counter 0 */
	_io_port.outb(PIT_CMD_PORT, PIT_CMD_READ_BACK |
	                            PIT_CMD_RB_COUNT |
	                            PIT_CMD_RB_STATUS |
	                            PIT_CMD_RB_CHANNEL_0);

	/* read status byte from latch register */
	uint8_t status = _io_port.inb(PIT_DATA_PORT_0);

	/* read low and high bytes from latch register */
	uint16_t lo = _io_port.inb(PIT_DATA_PORT_0);
	uint16_t hi = _io_port.inb(PIT_DATA_PORT_0);

	*wrapped = status & PIT_STAT_INT_LINE ? true : false;
	return (hi << 8) | lo;
}


void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	_handler = &handler;
	_timer_irq.ack_irq();
	unsigned long duration_us = duration.value;

	/* limit timer-interrupt rate */
	enum { MAX_TIMER_IRQS_PER_SECOND = 4*1000 };
	if (duration_us < 1000 * 1000 / MAX_TIMER_IRQS_PER_SECOND)
		duration_us = 1000 * 1000 / MAX_TIMER_IRQS_PER_SECOND;

	if (duration_us > max_timeout().value)
		duration_us = max_timeout().value;

	_counter_init_value = (PIT_TICKS_PER_MSEC * duration_us) / 1000;
	_set_counter(_counter_init_value);
}


Duration Timer::Time_source::curr_time()
{
	uint32_t passed_ticks;

	/*
	 * Read PIT count and status
	 *
	 * Reading the PIT registers via port I/O is a non-const operation.
	 * Since 'curr_time' is declared as const, however, we need to
	 * explicitly override the const-ness of the 'this' pointer.
	 */
	bool wrapped;
	uint16_t const curr_counter =
		const_cast<Time_source *>(this)->_read_counter(&wrapped);

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
	_curr_time_us += (passed_ticks*1000)/PIT_TICKS_PER_MSEC;

	/* use current counter as the reference for the next update */
	_counter_init_value = curr_counter;
	return Duration(Microseconds(_curr_time_us));
}


Timer::Time_source::Time_source(Env &env)
:
	Signalled_time_source(env),
	_io_port(env, PIT_DATA_PORT_0, PIT_CMD_PORT - PIT_DATA_PORT_0 + 1),
	_timer_irq(env, IRQ_PIT)
{
	/* operate PIT in one-shot mode */
	_io_port.outb(PIT_CMD_PORT, PIT_CMD_SELECT_CHANNEL_0 |
	              PIT_CMD_ACCESS_LO_HI | PIT_CMD_MODE_IRQ);

	_timer_irq.sigh(_signal_handler);
}
