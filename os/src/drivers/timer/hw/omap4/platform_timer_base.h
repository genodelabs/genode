/*
 * \brief  Basic driver behind platform timer
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _HW__OMAP4__PLATFORM_TIMER_BASE_H_
#define _HW__OMAP4__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <io_mem_session/connection.h>
#include <util/mmio.h>
#include <irq_session/connection.h>
#include <drivers/board.h>

namespace Genode
{
	/**
	 * Omap4 general purpose timer 3 through 9 and 11
	 */
	class Omap4_gp_timer_1 : public Mmio
	{
		/**
		 * Timer tics per microsecond
		 */
		static float tics_per_us() {
			return (float)Board::SYS_CLK / 1000000; }

		/**
		 * Microsecodns per timer tic
		 */
		static float us_per_tic() { return 1 / tics_per_us(); }

		/**
		 * L4 interface control
		 */
		struct Tiocp_cfg : Register<0x10, 32>
		{
			struct Softreset : Bitfield<0, 1> { }; /* SW reset active */
			struct Idlemode  : Bitfield<2, 2>      /* action on IDLE request */
			{
				enum { FORCE_IDLE = 0 };
			};
		};

		/**
		 * Timer wake-up enable register
		 */
		struct Twer : Register<0x20, 32>
		{
			struct Mat_wup_ena  : Bitfield<0, 1> { }; /* wakeup on match */
			struct Ovf_wup_ena  : Bitfield<1, 1> { }; /* wakeup on overflow */
			struct Tcar_wup_ena : Bitfield<2, 1> { }; /* wakeup on capture */

			/**
			 * Timer initialization value
			 */
			static access_t init_timer()
			{
				return Mat_wup_ena::bits(0) |
					   Ovf_wup_ena::bits(0) |
					   Tcar_wup_ena::bits(0);
			}
		};

		/**
		 * Timer synchronous interface control register
		 */
		struct Tsicr : Register<0x54, 32>
		{
			struct Posted : Bitfield<2, 1> { }; /* enable posted mode */
		};

		/**
		 * Control timer-functionality dependent features
		 */
		struct Tclr : Register<0x38, 32>
		{
			struct St  : Bitfield<0, 1> { }; /* start/stop timer */
			struct Ar  : Bitfield<1, 1> { }; /* enable eutoreload */
			struct Pre : Bitfield<5, 1> { }; /* enable prescaler */

			/**
			 * Run-and-wrap configuration
			 */
			static access_t init_run_and_wrap()
			{
				return St::bits(0) |
					   Ar::bits(1) |
					   Pre::bits(0);
			}
		};

		/**
		 * Set IRQ enables
		 */
		struct Irqenable_set : Register<0x2c, 32>
		{
			struct Ovf_en_flag : Bitfield<1, 1> { }; /* enable overflow IRQ */
		};

		/**
		 * IRQ status
		 */
		struct Irqstatus : Register<0x28, 32>
		{
			struct Ovf_it_flag : Bitfield<1, 1> { }; /* clear overflow IRQ */
		};

		/**
		 * Timer counter register
		 */
		struct Tcrr : Register<0x3c, 32>
		{
			/**
			 * maximum counter value
			 */
			static access_t max_value() { return ~0; }
		};

		/**
		 * Timer load value register
		 */
		struct Tldr : Register<0x40, 32> { };

		/**
		 * Freeze timer counter
		 */
		void _freeze() { write<Tclr::St>(0); }

		/**
		 * Unfreeze timer counter
		 */
		void _unfreeze() { write<Tclr::St>(1); }

		/**
		 * Get remaining counting amount
		 */
		unsigned long _value() { return max_value() - read<Tcrr>(); }

		/**
		 * Apply counting amount
		 */
		void _value(unsigned long const v) { write<Tcrr>(max_value() - v); }

		public:

			/**
			 * Constructor
			 *
			 * \param base  MMIO base
			 */
			Omap4_gp_timer_1(addr_t const base) : Mmio(base)
			{
				_freeze();

				/* do a software reset */
				write<Tiocp_cfg::Softreset>(1);
				while (read<Tiocp_cfg::Softreset>()) ;

				/* configure Idle mode */
				write<Tiocp_cfg::Idlemode>(Tiocp_cfg::Idlemode::FORCE_IDLE);

				/* enable wake-up interrupt events */
				write<Twer>(Twer::init_timer());

				/* select posted mode */
				write<Tsicr::Posted>(0);
			}

			/**
			 * Count down 'value', raise IRQ output, wrap counter and continue
			 */
			void run_and_wrap(unsigned long value)
			{
				enum { MIN_VALUE = 1 };

				/* stop timer */
				_freeze();
				clear_interrupt();
				value = value ? value : MIN_VALUE;

				/* configure for a run and wrap */
				write<Tclr>(Tclr::init_run_and_wrap());
				write<Irqenable_set::Ovf_en_flag>(1);

				/* install value */
				_value(value);
				write<Tldr>(0);

				/* start timer */
				_unfreeze();
			}

			/**
			 * Clear interrupt output
			 */
			void clear_interrupt() {
				write<Irqstatus::Ovf_it_flag>(1); }

			/**
			 * Maximum timeout value
			 */
			unsigned long max_value() { return Tcrr::max_value(); }

			/**
			 * Translate timer tics to microseconds
			 */
			unsigned long tics_to_us(unsigned long const tics)
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
			unsigned long value(bool & wrapped)
			{
				Tcrr::access_t const v = _value();
				wrapped = (bool)read<Irqstatus::Ovf_it_flag>();
				return wrapped ? _value() : v;
			}
	};
}

/**
 * Basic driver behind platform timer
 */
class Platform_timer_base : public Genode::Io_mem_connection,
                            public Genode::Omap4_gp_timer_1
{
	/* FIXME these should be located in a omap4-defs file */
	enum {
		GP_TIMER_3_IRQ       = 71,
		GP_TIMER_3_MMIO_BASE = 0x48034000,
		GP_TIMER_3_MMIO_SIZE = 0x00001000,
	};

	public:

		enum { IRQ = GP_TIMER_3_IRQ };

		/**
		 * Constructor
		 */
		Platform_timer_base() :
			Io_mem_connection(GP_TIMER_3_MMIO_BASE, GP_TIMER_3_MMIO_SIZE),
			Genode::Omap4_gp_timer_1((Genode::addr_t)Genode::env()->rm_session()->attach(dataspace()))
		{ }
};

#endif /* _HW__OMAP4__PLATFORM_TIMER_BASE_H_ */

