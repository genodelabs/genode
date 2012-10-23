/*
 * \brief  Core timer
 * \author Martin Stein
 * \author Norman Feske
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IMX31__TIMER_H_
#define _INCLUDE__IMX31__TIMER_H_

/* Genode includes */
#include <util/mmio.h>

namespace Imx31
{
	using namespace Genode;

	/**
	 * Core timer
	 */
	class Timer : public Mmio
	{
		enum { TICS_PER_MS = 32 };

		/**
		 * Control register
		 */
		struct Cr : Register<0x0, 32>
		{
			struct En : Bitfield<0, 1> { }; /* enable timer */

			struct En_mod : Bitfield<1, 1> /* reload or continue on enable */
			{
				enum { RELOAD = 1 };
			};

			struct Oci_en : Bitfield<2, 1> { }; /* interrupt on compare */

			struct Rld : Bitfield<3, 1> /* reload or roll-over */
			{
				enum { RELOAD_FROM_LR = 1 };
			};

			struct Prescaler : Bitfield<4, 12> /* clock input divisor */
			{
				enum { DIVIDE_BY_1 = 0 };
			};

			struct Swr     : Bitfield<16, 1> { }; /* software reset bit */
			struct Iovw    : Bitfield<17, 1> { }; /* enable overwrite */
			struct Dbg_en  : Bitfield<18, 1> { }; /* enable in debug mode */
			struct Wait_en : Bitfield<19, 1> { }; /* enable in wait mode */
			struct Doz_en  : Bitfield<20, 1> { }; /* enable in doze mode */
			struct Stop_en : Bitfield<21, 1> { }; /* enable in stop mode */

			struct Om : Bitfield<22, 2> /* mode of the output pin */
			{
				enum { DISCONNECTED = 0 };
			};

			struct Clk_src : Bitfield<24, 2> /* select clock input */
			{
				enum { IPG_CLK_32K = 3 };
			};

			/**
			 * Register value that configures the timer for a one-shot run
			 */
			static access_t prepare_one_shot()
			{
				return En::bits(0) |
				       En_mod::bits(En_mod::RELOAD) |
				       Oci_en::bits(1) |
				       Rld::bits(Rld::RELOAD_FROM_LR) |
				       Prescaler::bits(Prescaler::DIVIDE_BY_1) |
				       Swr::bits(0) |
				       Iovw::bits(0) |
				       Dbg_en::bits(0) |
				       Wait_en::bits(0) |
				       Doz_en::bits(0) |
				       Stop_en::bits(0) |
				       Om::bits(Om::DISCONNECTED) |
				       Clk_src::bits(Clk_src::IPG_CLK_32K);
			}
		};

		/**
		 * Status register
		 */
		struct Sr : Register<0x4, 32>
		{
			struct Ocif : Bitfield<0, 1> { }; /* IRQ status, write 1 clears */
		};

		struct Lr   : Register<0x8,  32> { }; /* load value register */
		struct Cmpr : Register<0xc,  32> { }; /* compare value register */
		struct Cnt  : Register<0x10, 32> { }; /* counter register */

		/**
		 * Disable timer and clear its interrupt output
		 */
		void _reset()
		{
			/* wait until ongoing reset operations are finished */
			while (read<Cr::Swr>()) ;

			/* disable timer */
			write<Cr::En>(0);
			clear_interrupt();
		}

		public:

			enum { IRQ = Board::EPIT_1_IRQ };

			/**
			 * Constructor
			 */
			Timer() : Mmio(Board::EPIT_1_MMIO_BASE) { _reset(); }

			/**
			 * Start a one-shot run
			 *
			 * \param tics  native timer value used to assess the delay
			 *              of the timer interrupt as of the call
			 */
			void start_one_shot(unsigned const tics)
			{
				/* stop timer */
				_reset();

				/* configure timer for a one-shot */
				write<Cr>(Cr::prepare_one_shot());
				write<Lr>(tics);
				write<Cmpr>(0);

				/* start timer */
				write<Cr::En>(1);
			}

			/**
			 * Stop the timer from a one-shot run
			 *
			 * \return  last native timer value of the one-shot run
			 */
			unsigned long stop_one_shot()
			{
				/* disable timer */
				write<Cr::En>(0);

				/* if the timer has hit zero already return 0 */
				return read<Sr::Ocif>() ? 0 : read<Cnt>();
			}

			/**
			 * Clear interrupt output line
			 */
			void clear_interrupt() { write<Sr::Ocif>(1); }

			/**
			 * Translate milliseconds to a native timer value
			 */
			static unsigned ms_to_tics(unsigned const ms) {
				return TICS_PER_MS * ms; }
	};
}

#endif /* _INCLUDE__IMX31__TIMER_H_ */

