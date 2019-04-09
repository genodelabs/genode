/*
 * \brief   A timer manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__TIMER_H_
#define _CORE__KERNEL__TIMER_H_

/* base-hw includes */
#include <kernel/types.h>
#include <kernel/irq.h>

/* Genode includes */
#include <util/list.h>

#include <board.h>

namespace Kernel
{
	class Cpu;
	class Timeout;
	class Timer;
}

/**
 * A timeout causes a kernel pass and the call of a timeout specific handle
 */
class Kernel::Timeout : Genode::List<Timeout>::Element
{
	friend class Timer;
	friend class Genode::List<Timeout>;

	private:

		bool   _listed     = false;
		time_t _end        = 0;

	public:

		/**
		 * Callback handle
		 */
		virtual void timeout_triggered() { }

		virtual ~Timeout() { }
};

/**
 * A timer manages a continuous time and timeouts on it
 */
class Kernel::Timer
{
	private:

		class Irq : private Kernel::Irq
		{
			private:

				Cpu & _cpu;

			public:

				Irq(unsigned id, Cpu & cpu);

				void occurred() override;
		};

		Board::Timer          _device;
		Irq                   _irq;
		time_t                _time = 0;
		time_t                _last_timeout_duration;
		Genode::List<Timeout> _timeout_list {};

		void _start_one_shot(time_t const ticks);

		time_t _value();

		time_t _max_value() const;

		time_t _duration() const;

	public:

		Timer(Cpu & cpu);

		/**
		 * Return duration from last call of this function
		 */
		time_t schedule_timeout();

		void process_timeouts();

		void set_timeout(Timeout * const timeout, time_t const duration);

		time_t us_to_ticks(time_t const us) const;

		time_t ticks_to_us(time_t const ticks) const;

		time_t timeout_max_us() const;

		unsigned interrupt_id() const;

		time_t time() const { return _time + _duration(); }
};

#endif /* _CORE__KERNEL__TIMER_H_ */
