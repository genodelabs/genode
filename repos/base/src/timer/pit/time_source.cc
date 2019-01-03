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

/* Genode includes */
#include <drivers/timer/util.h>

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
	unsigned long duration_us = duration.value;

	/* timeout '0' is trigger to cancel the current pending, if required */
	if (!duration.value) {
		duration_us = max_timeout().value;
		Signal_transmitter(_signal_handler).submit();
	} else {
		/* limit timer-interrupt rate */
		enum { MAX_TIMER_IRQS_PER_SECOND = 4*1000 };
		if (duration_us < 1000 * 1000 / MAX_TIMER_IRQS_PER_SECOND)
			duration_us = 1000 * 1000 / MAX_TIMER_IRQS_PER_SECOND;

		if (duration_us > max_timeout().value)
			duration_us = max_timeout().value;
	}

	_counter_init_value = (PIT_TICKS_PER_MSEC * duration_us) / 1000;
	_set_counter(_counter_init_value);

	if (duration.value)
		_timer_irq.ack_irq();
}


uint32_t Timer::Time_source::_ticks_since_update_no_wrap(uint16_t curr_counter)
{
	/*
	 * The counter did not wrap since the last update of _counter_init_value.
	 * This means that _counter_init_value is equal to or greater than
	 * curr_counter and that the time that passed is simply the difference
	 * between the two.
	 */
	return _counter_init_value - curr_counter;
}


uint32_t Timer::Time_source::_ticks_since_update_one_wrap(uint16_t curr_counter)
{
	/*
	 * The counter wrapped since the last update of _counter_init_value.
	 * This means that the time that passed is the whole _counter_init_value
	 * plus the time that passed since the counter wrapped.
	 */
	return _counter_init_value + PIT_MAX_COUNT - curr_counter;
}


Duration Timer::Time_source::curr_time()
{
	/* read out and update curr time solely if running in context of irq */
	if (_irq)
		_curr_time();

	return Duration(Microseconds(_curr_time_us));
}


Duration Timer::Time_source::_curr_time()
{
	/* read PIT counter and wrapped status */
	uint32_t ticks;
	bool wrapped;
	uint16_t const curr_counter = _read_counter(&wrapped);

	if (!wrapped) {

		/*
		 * The counter did not wrap since the last call to scheduled_timeout
		 * which means that it did not wrap since the last update of
		 * _counter_init_time.
		 */
		ticks = _ticks_since_update_no_wrap(curr_counter);
	}
	else if (wrapped && !_handled_wrap) {

		/*
		 * The counter wrapped at least once since the last call to
		 * schedule_timeout (wrapped) and curr_time (!_handled_wrap) which
		 * means that it definitely did wrap since the last update of
		 * _counter_init_time. We cannot determine whether it wrapped only
		 * once but we have to assume it. Even if it wrapped multiple times,
		 * the error that results from the assumption that it did not is pretty
		 * innocuous ((nr_of_wraps - 1) * 53 ms at a max).
		 */
		ticks = _ticks_since_update_one_wrap(curr_counter);
		_handled_wrap = true;
	}
	else { /* wrapped && _handled_wrap */

		/*
		 * The counter wrapped at least once since the last call to
		 * schedule_timeout (wrapped) but may not have wrapped since the last
		 * call to curr_time (_handled_wrap).
		 */

		if (_counter_init_value >= curr_counter) {

			/*
			 * We cannot determine whether the counter wrapped since the last
			 * call to curr_time but we have to assume that it did not. Even if
			 * it wrapped, the error that results from the assumption that it
			 * did not is pretty innocuous as long as _counter_init_value is
			 * not greater than curr_counter (nr_of_wraps * 53 ms at a max).
			 */
			ticks = _ticks_since_update_no_wrap(curr_counter);

		} else {

			/*
			 * The counter definitely wrapped multiple times since the last
			 * call to schedule_timeout and at least once since the last call
			 * to curr_time. It is the only explanation for the fact that
			 * curr_counter became greater than _counter_init_value again
			 * after _counter_init_value was updated with a wrapped counter
			 * by curr_time (_handled_wrap). This means two things:
			 *
			 * First, the counter wrapped at least once since the last update
			 * of _counter_init_value. We cannot determine whether it wrapped
			 * only once but we have to assume it. Even if it wrapped multiple
			 * times, the error that results from the assumption that it
			 * did not is pretty innocuous ((nr_of_wraps - 1) * 53 ms at a max).
			 *
			 * Second, we have to warn the user as it is a sure indication of
			 * insufficient activation latency if the counter wraps multiple
			 * times between two schedule_timeout calls.
			 */
			warning("PIT wrapped multiple times, timer-driver latency too big");
			ticks = _ticks_since_update_one_wrap(curr_counter);
		}
	}

	/* use current counter as the reference for the next update */
	_counter_init_value = curr_counter;

	/* translate counter to microseconds and update time value */
	static_assert(PIT_TICKS_PER_MSEC >= (unsigned)TIMER_MIN_TICKS_PER_MS,
	              "Bad TICS_PER_MS value");
	_curr_time_us += timer_ticks_to_us(ticks, PIT_TICKS_PER_MSEC);

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
