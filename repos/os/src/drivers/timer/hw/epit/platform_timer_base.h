/*
 * \brief  Basic driver behind platform timer
 * \author Stefan Kalkowski
 * \date   2012-10-25
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _HW__EPIT__PLATFORM_TIMER_BASE_H_
#define _HW__EPIT__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <io_mem_session/connection.h>
#include <util/mmio.h>
#include <irq_session/connection.h>
#include <drivers/board_base.h>
#include <drivers/timer/epit_base.h>

namespace Genode
{
	/**
	 * Epit timer
	 */
	class Epit : public Epit_base
	{
		private:

			/**
			 * Timer tics per microsecond
			 */
			static float tics_per_us() {
				return (float)TICS_PER_MS / 1000.0; }

			/**
			 * Microseconds per timer tic
			 */
			static float us_per_tic() { return 1.0 / tics_per_us(); }

		public:

			/**
			 * Constructor
			 *
			 * \param base  MMIO base
			 */
			Epit(addr_t const base) : Epit_base(base) { }

			/**
			 * Count down 'value', raise IRQ output, wrap counter and continue
			 */
			void run_and_wrap(unsigned long value) { _start_one_shot(value); }

			/**
			 * Maximum timeout value
			 */
			unsigned long max_value() const { return read<Lr>(); }

			/**
			 * Translate timer tics to microseconds
			 */
			unsigned long tics_to_us(unsigned long const tics) const
			{
				float const us = tics * us_per_tic();
				return (unsigned long)us;
			}

			/**
			 * Translate microseconds to timer tics
			 */
			unsigned long us_to_tics(unsigned long const us)
			{
				float const tics = us * tics_per_us();
				return (unsigned long)tics;
			}

			/**
			 * Sample the timer counter and according wrapped status
			 */
			unsigned long value(bool & wrapped) const
			{
				unsigned long v = read<Cnt>();
				wrapped = (bool)read<Sr::Ocif>();
				return wrapped ? read<Cnt>() : v;
			}
	};
}

/**
 * Basic driver behind platform timer
 */
class Platform_timer_base : public Genode::Io_mem_connection,
                            public Genode::Epit
{
	public:

	enum { IRQ = Genode::Board_base::EPIT_2_IRQ };

		/**
		 * Constructor
		 */
		Platform_timer_base()
		: Io_mem_connection(Genode::Board_base::EPIT_2_MMIO_BASE,
		                    Genode::Board_base::EPIT_2_MMIO_SIZE),
		  Genode::Epit((Genode::addr_t)Genode::env()->rm_session()
		                ->attach(dataspace()))
		{ }
};

#endif /* _HW__EPIT__PLATFORM_TIMER_BASE_H_ */

