/*
 * \brief  Timer for core
 * \author Martin stein
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER__EXYNOS_PWM_H_
#define _TIMER__EXYNOS_PWM_H_

/* Genode includes */
#include <util/mmio.h>

namespace Exynos_pwm
{
	using namespace Genode;

	/**
	 * Timer for core
	 *
	 * Exynos 5 PWM timer provides 5 independent 32 bit down count timers.
	 * This driver uses timer 0 only.
	 */
	class Timer : public Mmio
	{
		enum { PRESCALER = 2 };

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
				       Auto_reload0::bits(0) |
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

		float const _tics_per_ms;

		public:

			/**
			 * Constructor
			 */
			Timer(addr_t const base, unsigned const clk)
				: Mmio(base), _tics_per_ms((float)clk / PRESCALER / 1000)
			{
				write<Cfg0::Prescaler0>(Cfg0::Prescaler0::DEFAULT);
				write<Cfg1::Div0>(Cfg1::Div0::DISABLE);
				write<Int>(Int::init_value());
				write<Con>(Con::init_value());
				write<Cmpb0>(0);
			}

			/**
			 * Start a one-shot run
			 *
			 * \param  tics  native timer value used to assess the delay
			 *               of the timer interrupt as of the call
			 */
			inline void start_one_shot(uint32_t const tics)
			{
				write<Cntb0>(tics);
				write<Con::Enable0>(0);
				write<Con::Update0>(1);
				write<Con::Update0>(0);
				write<Con::Enable0>(1);
			}

			/**
			 * Translate milliseconds to a native timer value
			 */
			uint32_t ms_to_tics(unsigned const ms) {
				return ms * _tics_per_ms; }

			/**
			 * Stop the timer and return last timer value
			 */
			unsigned stop_one_shot() { return read<Cnto0>(); }

			/**
			 * Clear interrupt output line
			 */
			void clear_interrupt() { write<Int::Stat0>(1); }
	};
}

#endif /* _TIMER__EXYNOS_PWM_H_ */

