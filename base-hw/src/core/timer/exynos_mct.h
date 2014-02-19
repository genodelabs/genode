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

#ifndef _TIMER__EXYNOS_MCT_H_
#define _TIMER__EXYNOS_MCT_H_

/* Genode includes */
#include <util/mmio.h>

namespace Exynos_mct
{
	using namespace Genode;

	/**
	 * Timer for core
	 */
	class Timer : public Mmio
	{
		enum {
			PRESCALER = 1,
			DIV_MUX   = 0,
		};

		/**
		 * MCT configuration
		 */
		struct Mct_cfg : Register<0x0, 32>
		{
			struct Prescaler    : Bitfield<0, 8>  { };
			struct Div_mux      : Bitfield<8, 3>  { };
		};

		/**
		 * Local timer 0 free running counter buffer
		 */
		struct L0_frcntb : Register<0x310, 32> { };

		/**
		 * Local timer 0 configuration
		 */
		struct L0_tcon : Register<0x320, 32>
		{
			struct Frc_start : Bitfield<3, 1> { };
		};

		/**
		 * Local timer 0 expired status
		 */
		struct L0_int_cstat : Register<0x330, 32, true>
		{
			struct Frcnt : Bitfield<1, 1> { };
		};

		/**
		 * Local timer 0 interrupt enable
		 */
		struct L0_int_enb : Register<0x334, 32>
		{
			struct Frceie : Bitfield<1, 1> { };
		};

		/**
		 * Local timer 0 write status
		 */
		struct L0_wstat : Register<0x340, 32, true>
		{
			struct Frcntb : Bitfield<2, 1> { };
			struct Tcon   : Bitfield<3, 1> { };
		};

		/**
		 * Write to reg that replies via ack bit and clear ack bit
		 */
		template <typename DEST, typename ACK>
		void _acked_write(typename DEST::Register_base::access_t const v)
		{
			typedef typename DEST::Register_base Dest;
			typedef typename ACK::Bitfield_base  Ack;
			write<Dest>(v);
			while (!read<Ack>());
			write<Ack>(1);
		}

		unsigned long const _tics_per_ms;

		/**
		 * Start and stop counting
		 */
		void _run(bool const run)
		{
			_acked_write<L0_tcon, L0_wstat::Tcon>
				(L0_tcon::Frc_start::bits(run));
		}

		public:

			/**
			 * Constructor
			 */
			Timer(addr_t const base, unsigned const clk)
			: Mmio(base), _tics_per_ms(clk / (PRESCALER + 1) / (1 << DIV_MUX) / 1000)
			{
				Mct_cfg::access_t mct_cfg = 0;
				Mct_cfg::Prescaler::set(mct_cfg, PRESCALER);
				Mct_cfg::Div_mux::set(mct_cfg, DIV_MUX);
				write<Mct_cfg>(mct_cfg);
				write<L0_int_enb>(L0_int_enb::Frceie::bits(1));
			}

			/**
			 * Start one-shot run with an IRQ delay of 'tics'
			 */
			inline void start_one_shot(unsigned const tics)
			{
				_run(0);
				_acked_write<L0_frcntb, L0_wstat::Frcntb>(tics);
				_run(1);
			}

			/**
			 * Translate 'ms' milliseconds to a native timer value
			 */
			unsigned ms_to_tics(unsigned const ms)
			{
				return ms * _tics_per_ms;
			}

			/**
			 * Clear interrupt output line
			 */
			void clear_interrupt() { write<L0_int_cstat::Frcnt>(1); }
	};
}

#endif /* _TIMER__EXYNOS_MCT_H_ */
