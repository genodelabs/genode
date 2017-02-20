/*
 * \brief  Timer driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__IMX53__TIMER_H_
#define _CORE__INCLUDE__SPEC__IMX53__TIMER_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>
#include <platform.h>

namespace Genode { class Timer; }

/**
 * Timer driver for core
 */
class Genode::Timer : public Mmio
{
	private:

		using time_t = Kernel::time_t;

		enum { TICS_PER_MS = 33333 };

		/**
		 * Control register
		 */
		struct Cr : Register<0x0, 32>
		{
			struct En : Bitfield<0, 1> { };       /* enable timer */

			struct En_mod : Bitfield<1, 1>        /* reload on enable */
			{
				enum { RELOAD = 1 };
			};

			struct Oci_en : Bitfield<2, 1> { };   /* interrupt on compare */

			struct Rld : Bitfield<3, 1>           /* reload or roll-over */
			{
				enum { RELOAD_FROM_LR = 1 };
			};

			struct Prescaler : Bitfield<4, 12>    /* clock input divisor */
			{
				enum { DIVIDE_BY_1 = 0 };
			};

			struct Swr     : Bitfield<16, 1> { }; /* software reset bit */
			struct Iovw    : Bitfield<17, 1> { }; /* enable overwrite */
			struct Dbg_en  : Bitfield<18, 1> { }; /* enable in debug mode */
			struct Wait_en : Bitfield<19, 1> { }; /* enable in wait mode */
			struct Doz_en  : Bitfield<20, 1> { }; /* enable in doze mode */
			struct Stop_en : Bitfield<21, 1> { }; /* enable in stop mode */

			struct Om : Bitfield<22, 2>           /* mode of the output pin */
			{
				enum { DISCONNECTED = 0 };
			};

			struct Clk_src : Bitfield<24, 2>      /* select clock input */
			{
				enum { HIGH_FREQ_REF_CLK = 2 };
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
				       Clk_src::bits(Clk_src::HIGH_FREQ_REF_CLK);
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

			/* clear interrupt */
			write<Sr::Ocif>(1);
		}

	public:

		/**
		 * Return kernel name of timer interrupt
		 */
		static unsigned interrupt_id(unsigned const)
		{
			return Board::EPIT_1_IRQ;
		}

		Timer() : Mmio(Platform::mmio_to_virt(Board::EPIT_1_MMIO_BASE)) { }

		/**
		 * Start single timeout run
		 *
		 * \param tics  delay of timer interrupt
		 */
		void start_one_shot(time_t const tics, unsigned const)
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

		time_t tics_to_us(time_t const tics) const {
			return (tics / TICS_PER_MS) * 1000; }

		time_t us_to_tics(time_t const us) const {
			return (us / 1000) * TICS_PER_MS; }

		time_t max_value() { return (Cnt::access_t)~0; }

		/**
		 * Return current native timer value
		 */
		time_t value(unsigned const) {
			return read<Sr::Ocif>() ? 0 : read<Cnt>(); }
};

namespace Kernel { using Timer = Genode::Timer; }

#endif /* _CORE__INCLUDE__SPEC__IMX53__TIMER_H_ */
