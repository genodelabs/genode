/*
 * \brief  Timer driver for core
 * \author Martin stein
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* core include */
#include <board.h>

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	/**
	 * Timer driver for core
	 */
	class Timer;
}

class Genode::Timer : public Mmio
{
	private:

		enum {
			PRESCALER = 1,
			DIV_MUX   = 0,
		};

		/**
		 * MCT configuration
		 */
		struct Mct_cfg : Register<0x0, 32>
		{
			struct Prescaler : Bitfield<0, 8>  { };
			struct Div_mux   : Bitfield<8, 3>  { };
		};


		/*******************
		 ** Local timer 0 **
		 *******************/

		/**
		 * Free running counter buffer
		 */
		struct L0_frcntb : Register<0x310, 32> { };

		/**
		 * Configuration
		 */
		struct L0_tcon : Register<0x320, 32>
		{
			struct Frc_start : Bitfield<3, 1> { };
		};

		/**
		 * Expired status
		 */
		struct L0_int_cstat : Register<0x330, 32, true>
		{
			struct Frcnt : Bitfield<1, 1> { };
		};

		/**
		 * Interrupt enable
		 */
		struct L0_int_enb : Register<0x334, 32>
		{
			struct Frceie : Bitfield<1, 1> { };
		};

		/**
		 * Write status
		 */
		struct L0_wstat : Register<0x340, 32, true>
		{
			struct Frcntb : Bitfield<2, 1> { };
			struct Tcon   : Bitfield<3, 1> { };
		};

		struct L0_frcnto : Register<0x314, 32> { };

		/**
		 * Start and stop counting
		 */
		void _run_0(bool const run)
		{
			_acked_write<L0_tcon, L0_wstat::Tcon>
				(L0_tcon::Frc_start::bits(run));
		}


		/*******************
		 ** Local timer 1 **
		 *******************/

		/**
		 * Free running counter buffer
		 */
		struct L1_frcntb : Register<0x410, 32> { };

		/**
		 * Configuration
		 */
		struct L1_tcon : Register<0x420, 32>
		{
			struct Frc_start : Bitfield<3, 1> { };
		};

		/**
		 * Expired status
		 */
		struct L1_int_cstat : Register<0x430, 32, true>
		{
			struct Frcnt : Bitfield<1, 1> { };
		};

		/**
		 * Interrupt enable
		 */
		struct L1_int_enb : Register<0x434, 32>
		{
			struct Frceie : Bitfield<1, 1> { };
		};

		/**
		 * Write status
		 */
		struct L1_wstat : Register<0x440, 32, true>
		{
			struct Frcntb : Bitfield<2, 1> { };
			struct Tcon   : Bitfield<3, 1> { };
		};

		struct L1_frcnto : Register<0x414, 32> { };

		/**
		 * Start and stop counting
		 */
		void _run_1(bool const run)
		{
			_acked_write<L1_tcon, L1_wstat::Tcon>
				(L1_tcon::Frc_start::bits(run));
		}


		/********************
		 ** Helper methods **
		 ********************/

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

		/**
		 * Calculate amount of tics per ms for specific input clock
		 *
		 * \param clock  input clock
		 */
		unsigned static _calc_tics_per_ms(unsigned const clock)
		{
			return clock / (PRESCALER + 1) / (1 << DIV_MUX) / 1000;
		}

		unsigned const _tics_per_ms;

	public:


		/**
		 * Return kernel name of timer interrupt of a specific processor
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		static unsigned interrupt_id(unsigned const processor_id)
		{
			switch (processor_id) {
			case 0:  return Board::MCT_IRQ_L0;
			case 1:  return Board::MCT_IRQ_L1;
			default: return 0;
			}
		}

		/**
		 * Constructor
		 */
		Timer()
		:
			Mmio(Board::MCT_MMIO_BASE),
			_tics_per_ms(_calc_tics_per_ms(Board::MCT_CLOCK))
		{
			Mct_cfg::access_t mct_cfg = 0;
			Mct_cfg::Prescaler::set(mct_cfg, PRESCALER);
			Mct_cfg::Div_mux::set(mct_cfg, DIV_MUX);
			write<Mct_cfg>(mct_cfg);
			write<L0_int_enb>(L0_int_enb::Frceie::bits(1));
			write<L1_int_enb>(L1_int_enb::Frceie::bits(1));
		}

		/**
		 * Start single timeout run
		 *
		 * \param tics          delay of timer interrupt
		 * \param processor_id  kernel name of processor of targeted timer
		 */
		inline void start_one_shot(unsigned const tics,
		                           unsigned const processor_id)
		{
			switch (processor_id) {
			case 0:
				_run_0(0);
				_acked_write<L0_frcntb, L0_wstat::Frcntb>(tics);
				_run_0(1);
				return;
			case 1:
				_run_1(0);
				_acked_write<L1_frcntb, L1_wstat::Frcntb>(tics);
				_run_1(1);
				return;
			default: return;
			}
		}

		/**
		 * Translate 'ms' milliseconds to a native timer value
		 */
		unsigned ms_to_tics(unsigned const ms) { return ms * _tics_per_ms; }

		/**
		 * Clear interrupt output line
		 */
		void clear_interrupt(unsigned const processor_id)
		{
			switch (processor_id) {
			case 0:
				write<L0_int_cstat::Frcnt>(1);
				return;
			case 1:
				write<L1_int_cstat::Frcnt>(1);
				return;
			default: return;
			}
		}

		unsigned value(unsigned const processor_id)
		{
			switch (processor_id) {
			case 0:  return read<L0_frcnto>();
			case 1:  return read<L1_frcnto>();
			default: return 0;
			}
		}

		unsigned irq_state(unsigned const processor_id)
		{
			switch (processor_id) {
			case 0:  return read<L0_int_cstat::Frcnt>();
			case 1:  return read<L1_int_cstat::Frcnt>();
			default: return 0;
			}
		}
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
