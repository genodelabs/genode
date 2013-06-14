/*
 * \brief  Regulator driver for clock management unit of Exynos5250 SoC
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CMU_H_
#define _CMU_H_

#include <regulator/consts.h>
#include <regulator/driver.h>
#include <drivers/board_base.h>
#include <os/attached_io_mem_dataspace.h>
#include <util/mmio.h>

using namespace Regulator;


class Cmu : public Regulator::Driver,
            public Genode::Attached_io_mem_dataspace,
            public Genode::Mmio
{
	private:


		/***********************
		 ** CMU CPU registers **
		 ***********************/

		struct Apll_lock : Register<0x000, 32>
		{
			struct Pll_locktime : Bitfield <0, 20> { };

			static access_t max_lock_time(access_t pdiv) { return pdiv * 250; };
		};

		struct Apll_con0 : Register<0x100, 32>
		{
			struct S : Bitfield <0, 3>
			{
				/* S values for frequencies 200 - 1700 */
				static const Genode::uint8_t values[];
			};

			struct P : Bitfield <8, 6>
			{
				/* P values for frequencies 200 - 1700 */
				static const Genode::uint8_t values[];
			};

			struct M : Bitfield <16, 10>
			{
				/* M values for frequencies 200 - 1700 */
				static const Genode::uint16_t values[];
			};

			struct Locked : Bitfield <29, 1> { };
		};

		struct Clk_src_cpu : Register<0x200, 32>
		{
			struct Mux_cpu_sel : Bitfield<16, 1>
			{
				enum { MOUT_APLL, SCLK_MPLL};
			};
		};

		struct Clk_mux_stat_cpu : Register<0x400, 32>
		{
			struct Cpu_sel : Bitfield<16, 3>
			{
				enum { MOUT_APLL = 0b1, SCLK_MPLL = 0b10 };
			};
		};

		struct Clk_div_cpu0 : Register<0x500, 32>
		{
			/* Cpu0 divider values for frequencies 200 - 1700 */
			static const Genode::uint32_t values[];
		};

		struct Clk_div_cpu1 : Register<0x504, 32>
		{
			/* Divider for cpu1 doesn't change */
			enum { FIX_VALUE = 32 };
		};

		struct Clk_div_stat_cpu0 : Register<0x600, 32>
		{
			struct Div_arm      : Bitfield< 0, 1> {};
			struct Div_cpud     : Bitfield< 4, 1> {};
			struct Div_acp      : Bitfield< 8, 1> {};
			struct Div_pheriph  : Bitfield<12, 1> {};
			struct Div_atb      : Bitfield<16, 1> {};
			struct Div_pclk_dbg : Bitfield<20, 1> {};
			struct Div_apll     : Bitfield<24, 1> {};
			struct Div_arm2     : Bitfield<28, 1> {};

			static bool in_progress(access_t stat_word)
			{
				return stat_word & (Div_arm::bits(1)      |
				                    Div_cpud::bits(1)     |
				                    Div_acp::bits(1)      |
				                    Div_pheriph::bits(1)  |
				                    Div_atb::bits(1)      |
				                    Div_pclk_dbg::bits(1) |
				                    Div_apll::bits(1)     |
				                    Div_arm2::bits(1));
			}
		};

		struct Clk_div_stat_cpu1 : Register<0x604, 32>
		{
			struct Div_copy : Bitfield<0, 1> { };
			struct Div_hpm  : Bitfield<4, 1> { };

			static bool in_progress(access_t stat_word)
			{
				return stat_word & (Div_copy::bits(1) |
				                    Div_hpm::bits(1));
			}
		};


		/**
		 * CPU frequency scaling function
		 */
		void _cpu_clk_freq(Cpu_clock_freq freq)
		{
			/**
			 * change clock divider values
			 */

			/* cpu0 divider */
			write<Clk_div_cpu0>(Clk_div_cpu0::values[freq]);
			while (Clk_div_stat_cpu0::in_progress(read<Clk_div_stat_cpu0>())) ;

			/* cpu1 divider */
			write<Clk_div_cpu1>(Clk_div_cpu1::FIX_VALUE);
			while (Clk_div_stat_cpu1::in_progress(read<Clk_div_stat_cpu1>())) ;


			/**
			 * change APLL frequency
			 */

			/* change reference clock to MPLL */
			write<Clk_src_cpu::Mux_cpu_sel>(Clk_src_cpu::Mux_cpu_sel::SCLK_MPLL);
			while (read<Clk_mux_stat_cpu::Cpu_sel>()
			       != Clk_mux_stat_cpu::Cpu_sel::SCLK_MPLL) ;

			/* set lock time */
			unsigned pdiv = Apll_con0::P::values[freq];
			write<Apll_lock::Pll_locktime>(Apll_lock::max_lock_time(pdiv));

			/* change P, M, S values of APLL */
			write<Apll_con0::P>(Apll_con0::P::values[freq]);
			write<Apll_con0::M>(Apll_con0::M::values[freq]);
			write<Apll_con0::S>(Apll_con0::S::values[freq]);

			while (!read<Apll_con0::Locked>()) ;

			/* change reference clock back to APLL */
			write<Clk_src_cpu::Mux_cpu_sel>(Clk_src_cpu::Mux_cpu_sel::MOUT_APLL);
			while (read<Clk_mux_stat_cpu::Cpu_sel>()
			       != Clk_mux_stat_cpu::Cpu_sel::MOUT_APLL) ;
		}

	public:

		/**
		 * Constructor
		 */
		Cmu()
		: Genode::Attached_io_mem_dataspace(Genode::Board_base::CMU_MMIO_BASE,
		                                    Genode::Board_base::CMU_MMIO_SIZE),
			Mmio((Genode::addr_t)local_addr<void>())
		{
			/* set CPU to full speed */
			_cpu_clk_freq(CPU_FREQ_1600);
		}


		/********************************
		 ** Regulator driver interface **
		 ********************************/

		void set_level(Regulator_id id, unsigned long level)
		{
			switch (id) {
			case CLK_CPU:
				if (level >= CPU_FREQ_MAX) {
					PWRN("level=%ld not supported", level);
					return;
				}
				_cpu_clk_freq(static_cast<Cpu_clock_freq>(level));
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
		}

		unsigned long level(Regulator_id id)
		{
			switch (id) {
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
			return 0;
		}

		void set_state(Regulator_id id, bool enable)
		{
			switch (id) {
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
		}

		bool state(Regulator_id id)
		{
			switch (id) {
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
			return true;
		}
};


const Genode::uint8_t Cmu::Apll_con0::S::values[]  = { 2, 1, 1, 0, 0, 0, 0, 0, 0 };
const Genode::uint16_t Cmu::Apll_con0::M::values[] = { 100, 100, 200, 100, 125,
                                                       150, 175, 200, 425 };
const Genode::uint8_t Cmu::Apll_con0::P::values[]  = { 3, 3, 4, 3, 3, 3, 3, 3, 6 };
const Genode::uint32_t Cmu::Clk_div_cpu0::values[] = { 0x1117710, 0x1127710, 0x1137710,
                                                       0x2147710, 0x2147710, 0x3157720,
                                                       0x4167720, 0x4177730, 0x5377730 };
#endif /* _CMU_H_ */
