/*
 * \brief  Basic driver behind platform timer
 * \author Martin stein
 * \date   2013-04-04
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _HW__EXYNOS5__PLATFORM_TIMER_BASE_H_
#define _HW__EXYNOS5__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <io_mem_session/connection.h>
#include <irq_session/connection.h>
#include <drivers/board_base.h>
#include <util/mmio.h>

namespace Genode
{
	/**
	 * Exynos 5250 pulse width modulation timer
	 */
	class Pwm : public Mmio
	{
		enum {
			PRESCALER   = 2,
			TICS_PER_US = Board_base::PWM_CLOCK / PRESCALER / 1000 / 1000,
		};

		/**
		 * Timer configuration 0
		 */
		struct Cfg0 : Register<0x0, 32>
		{
			struct Prescaler0 : Bitfield<0, 8>
			{
				enum { DEFAULT = PRESCALER - 1 };
			};
		};

		/**
		 * Timer configuration 1
		 */
		struct Cfg1 : Register<0x4, 32>
		{
			struct Div0 : Bitfield<0, 4> { enum { DISABLE = 0 }; };
		};

		/**
		 * Timer control
		 */
		struct Con : Register<0x8, 32>
		{
			struct Enable0      : Bitfield<0, 1> { };
			struct Update0      : Bitfield<1, 1> { };
			struct Invert_tout0 : Bitfield<2, 1> { };
			struct Auto_reload0 : Bitfield<3, 1> { };
			struct Deadzone_en  : Bitfield<4, 1> { };

			/**
			 * Initialization value
			 */
			static access_t init_value()
			{
				return Invert_tout0::bits(0) |
				       Auto_reload0::bits(1) |
				       Deadzone_en::bits(0);
			}
		};

		/**
		 * Timer 0 count buffer
		 */
		struct Cntb0 : Register<0xc, 32> { };

		/**
		 * Timer 0 compare buffer
		 */
		struct Cmpb0 : Register<0x10, 32> { };

		/**
		 * Timer 0 count observation
		 */
		struct Cnto0 : Register<0x14, 32> { };

		/**
		 * Timer IRQ control and status
		 */
		struct Int : Register<0x44, 32>
		{
			struct En0   : Bitfield<0, 1> { };
			struct En1   : Bitfield<1, 1> { };
			struct En2   : Bitfield<2, 1> { };
			struct En3   : Bitfield<3, 1> { };
			struct En4   : Bitfield<4, 1> { };
			struct Stat0 : Bitfield<5, 1> { };

			/**
			 * Initialization value
			 */
			static access_t init_value()
			{
				return En0::bits(1) |
				       En1::bits(0) |
				       En2::bits(0) |
				       En3::bits(0) |
				       En4::bits(0);
			}
		};

		public:

			/**
			 * Constructor
			 *
			 * \param base  MMIO base
			 */
			Pwm(addr_t const base) : Mmio(base)
			{
				write<Cfg0::Prescaler0>(Cfg0::Prescaler0::DEFAULT);
				write<Cfg1::Div0>(Cfg1::Div0::DISABLE);
				write<Int>(Int::init_value());
				write<Con>(Con::init_value());
				write<Cmpb0>(0);
			}

			/**
			 * Count down 'value', raise IRQ output, wrap counter and continue
			 */
			void run_and_wrap(unsigned long value)
			{
				write<Cntb0>(value);
				write<Con::Enable0>(0);
				write<Con::Update0>(1);
				write<Con::Update0>(0);
				write<Int::Stat0>(1);
				write<Cntb0>(max_value());
				write<Con::Enable0>(1);
			}

			/**
			 * Maximum timeout value
			 */
			unsigned long max_value() const { return (Cntb0::access_t)~0; }

			/**
			 * Translate timer tics to microseconds
			 */
			unsigned long tics_to_us(unsigned long const tics) const {
				return tics / TICS_PER_US; }

			/**
			 * Translate microseconds to timer tics
			 */
			unsigned long us_to_tics(unsigned long const us) const {
				return us * TICS_PER_US; }

			/**
			 * Sample the timer counter and according wrapped status
			 */
			unsigned long value(bool & wrapped) const
			{
				unsigned long v = read<Cnto0>();
				wrapped = (bool)read<Int::Stat0>();
				return wrapped ? read<Cnto0>() : v;
			}
	};
}

/**
 * Basic driver behind platform timer
 */
class Platform_timer_base : public Genode::Io_mem_connection,
                            public Genode::Pwm
{
	public:

	enum { IRQ = Genode::Board_base::PWM_IRQ_0 };

		/**
		 * Constructor
		 */
		Platform_timer_base()
		: Io_mem_connection(Genode::Board_base::PWM_MMIO_BASE,
		                    Genode::Board_base::PWM_MMIO_SIZE),
		  Genode::Pwm((Genode::addr_t)Genode::env()->rm_session()
		              ->attach(dataspace()))
		{ }
};

#endif /* _HW__EXYNOS5__PLATFORM_TIMER_BASE_H_ */

