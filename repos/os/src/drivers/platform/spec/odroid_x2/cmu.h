/*
 * \brief  Regulator driver for clock management unit of Exynos4412 SoC
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopez Leon <humberto@uclv.cu>
 * \author Reinier Millo Sanchez <rmillo@uclv.cu>
 * \date   2015-07-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__ODROID_X2__CMU_H_
#define _DRIVERS__PLATFORM__SPEC__ODROID_X2__CMU_H_

#include <regulator/consts.h>
#include <regulator/driver.h>
#include <drivers/board_base.h>
#include <os/attached_mmio.h>
#include <base/log.h>

using namespace Regulator;


class Cmu : public Regulator::Driver,
            public Genode::Attached_mmio
{
	private:

		static const Genode::uint16_t m_values[]; /* M values for frequencies */
		static const Genode::uint8_t  p_values[]; /* P values for frequencies */
		static const Genode::uint8_t  s_values[]; /* S values for frequencies */

		template <unsigned OFF>
		struct Pll_lock : Register<OFF, 32>
		{
			struct Pll_locktime : Register<OFF, 32>::template Bitfield<0, 20> { };

			static Genode::uint32_t max_lock_time(Genode::uint8_t pdiv) {
				return pdiv * 250; };
		};

		template <unsigned OFF>
		struct Pll_con0 : Register<OFF, 32>
		{
			struct S      : Register<OFF, 32>::template Bitfield < 0,  3> { };
			struct P      : Register<OFF, 32>::template Bitfield < 8,  6> { };
			struct M      : Register<OFF, 32>::template Bitfield <16, 10> { };
			struct Locked : Register<OFF, 32>::template Bitfield <29,  1> { };
			struct Enable : Register<OFF, 32>::template Bitfield <31,  1> { };
		};


		/***********************
		 ** CMU CPU registers **
		 ***********************/
		typedef Pll_lock<4000>   Apll_lock;
		typedef Pll_con0<0x14100> Apll_con0;


		struct Clk_src_cpu : Register<0x14200, 32>
		{
			struct Mux_core_sel : Bitfield<16, 1>
			{
				enum { MOUT_APLL, SCLK_MPLL};
			};
		};

		struct Clk_mux_stat_cpu : Register<0x14400, 32>
		{

			struct Core_sel : Bitfield<16, 3>
			{
				enum { MOUT_APLL = 0b1, SCLK_MPLL = 0b10 };
			};
		};

		struct Clk_div_cpu0 : Register<0x14500, 32>
		{
			/* Cpu0 divider values for frequencies 200 - 1400 */
			static const Genode::uint32_t values[];
		};

		struct Clk_div_cpu1 : Register<0x14504, 32>
		{
			/* Divider for cpu1 doesn't change */
			enum { FIX_VALUE = 32 };
		};

		struct Clk_div_stat_cpu0 : Register<0x14600, 32>
		{

			struct Div_core      : Bitfield< 0, 1> {};
			struct Div_corem0     : Bitfield< 4, 1> {};
			struct Div_corem1      : Bitfield< 8, 1> {};
			struct Div_pheriph  : Bitfield<12, 1> {};
			struct Div_atb      : Bitfield<16, 1> {};
			struct Div_pclk_dbg : Bitfield<20, 1> {};
			struct Div_apll     : Bitfield<24, 1> {};
			struct Div_core2     : Bitfield<28, 1> {};


			static bool in_progress(access_t stat_word)
			{
				return stat_word & (Div_core::bits(1)      |
				                    Div_corem0::bits(1)     |
				                    Div_corem1::bits(1)      |
				                    Div_pheriph::bits(1)  |
				                    Div_atb::bits(1)      |
				                    Div_pclk_dbg::bits(1) |
				                    Div_apll::bits(1)     |
				                    Div_core2::bits(1));
			}
		};

		struct Clk_div_stat_cpu1 : Register<0x14604, 32>
		{
			struct Div_copy : Bitfield<0, 1> { };
			struct Div_hpm  : Bitfield<4, 1> { };

			static bool in_progress(access_t stat_word)
			{
				return stat_word & (Div_copy::bits(1) |
				                    Div_hpm::bits(1));
			}
		};


		/************************
		 ** CMU CORE registers **
		 ************************/

		typedef Pll_lock<0x0008> Mpll_lock;
		typedef Pll_con0<0x0108> Mpll_con0;

		/***********************
		 ** CMU TOP registers **
		 ***********************/
		struct Clk_gate_ip_tv : Register<0x10928, 32>
		{
			struct Clk_mixer     : Bitfield<1, 1> { };
			struct Clk_hdmi      : Bitfield<3, 1> { };
		};

		struct Clk_gate_ip_fsys : Register<0xC940, 32>
		{
			struct Usbhost20         : Bitfield<12, 1> { };
			struct Usbdevice	 : Bitfield<13, 1> { };

		};

		struct Clk_src_tv : Register<0xC224, 32> /* old name Clk_src_disp1_0 */
		{
			struct Hdmi_sel : Bitfield<0, 1> { };
		};

		struct Clk_src_mask_tv : Register<0xC324, 32>
		{
			struct Hdmi_mask : Bitfield<0, 1> { };
		};

		struct Clk_gate_ip_peric : Register<0xC950, 32>
		{
			struct Clk_uart2   : Bitfield<2,  1> { };
			struct Clk_i2chdmi : Bitfield<14, 1> { };
			struct Clk_pwm     : Bitfield<24, 1> { };
		};

		struct Clk_gate_block : Register<0xC970, 32>
		{
			struct Clk_tv 	 : Bitfield<1, 1> { };
		};

		/*******************
		 ** CPU functions **
		 *******************/

		Cpu_clock_freq _cpu_freq;

		void _cpu_clk_freq(unsigned long level)
		{
			using namespace Genode;
			log("Changing CPU frequency to ",level);
			unsigned freq;
			switch (level) {
			case CPU_FREQ_200:
				freq = 0;
				break;
			case CPU_FREQ_400:
				freq = 1;
				break;
			case CPU_FREQ_600:
				freq = 2;
				break;
			case CPU_FREQ_800:
				freq = 3;
				break;
			case CPU_FREQ_1000:
				freq = 4;
				break;
			case CPU_FREQ_1200:
				freq = 5;
				break;
			case CPU_FREQ_1400:
				freq = 6;
				break;
			default:
				warning("Unsupported CPU frequency level ", level);
				warning("Supported values are 200, 400, 600, 800, 1000, 1200, 14000 MHz");
				warning("and 1, 1.2, 1.4, 1.6, 1.7 GHz");
				return;
			};

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
			write<Clk_src_cpu::Mux_core_sel>(Clk_src_cpu::Mux_core_sel::SCLK_MPLL);
			while (read<Clk_mux_stat_cpu::Core_sel>()
			       != Clk_mux_stat_cpu::Core_sel::SCLK_MPLL) ;

			/* set lock time */
			unsigned pdiv = p_values[freq];
			write<Apll_lock::Pll_locktime>(Apll_lock::max_lock_time(pdiv));

			/* change P, M, S values of APLL */
			write<Apll_con0::P>(p_values[freq]);
			write<Apll_con0::M>(m_values[freq]);
			write<Apll_con0::S>(s_values[freq]);

			while (!read<Apll_con0::Locked>()) ;

			/* change reference clock back to APLL */
			write<Clk_src_cpu::Mux_core_sel>(Clk_src_cpu::Mux_core_sel::MOUT_APLL);
			while (read<Clk_mux_stat_cpu::Core_sel>()
			       != Clk_mux_stat_cpu::Core_sel::MOUT_APLL) ;

			_cpu_freq = static_cast<Cpu_clock_freq>(level);
			Genode::log("changed CPU frequency to ",level);
		}


		/**********************
		 ** Device functions **
		 **********************/

		void _hdmi_enable()
		{

			write<Clk_gate_ip_peric::Clk_i2chdmi>(1);

			Clk_gate_ip_tv::access_t gd1 = read<Clk_gate_ip_tv>();
			Clk_gate_ip_tv::Clk_mixer::set(gd1, 1);
			Clk_gate_ip_tv::Clk_hdmi::set(gd1, 1);
			write<Clk_gate_ip_tv>(gd1);
			write<Clk_gate_block::Clk_tv>(1);
			write<Clk_src_mask_tv::Hdmi_mask>(1);
			write<Clk_src_tv::Hdmi_sel>(1);

		}

		void _enable(Regulator_id id)
		{
			switch (id) {
			case CLK_USB20:
			    {
			     write<Clk_gate_ip_fsys::Usbdevice>(1);
			     return write<Clk_gate_ip_fsys::Usbhost20>(1);
			    }

			case CLK_HDMI:
				_hdmi_enable();
				break;
			default:
				Genode::warning("enabling regulator unsupported for ", names[id].name);
			}
		}

		void _disable(Regulator_id id)
		{
			switch (id) {
			case CLK_USB20:
			{
				write<Clk_gate_ip_fsys::Usbdevice>(0);
				return write<Clk_gate_ip_fsys::Usbhost20>(0);
			}
			default:
				Genode::warning("disabling regulator unsupported for ", names[id].name);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Cmu()
		: Genode::Attached_mmio(Genode::Board_base::CMU_MMIO_BASE,
		                        Genode::Board_base::CMU_MMIO_SIZE),
		  _cpu_freq(CPU_FREQ_1400)
		{
			/**
			 * Close certain clock gates by default (~ 0.7 Watt reduction)
			 */
			write<Clk_gate_ip_fsys>(0);
			write<Clk_gate_ip_peric::Clk_uart2>(1);
			write<Clk_gate_ip_peric::Clk_pwm>(1);

			/**
			 * Set default CPU frequency
			 */
			_cpu_clk_freq(_cpu_freq);

		}
		/********************************
		 ** Regulator driver interface **
		 ********************************/

		void level(Regulator_id id, unsigned long level)
		{
			switch (id) {
			case CLK_CPU:
				_cpu_clk_freq(level);
				break;
			default:
				Genode::warning("level setting unsupported for ", names[id].name);
			}
		}

		unsigned long level(Regulator_id id)
		{
			switch (id) {
			case CLK_CPU:
				return _cpu_freq;
			default:
				Genode::warning("level requesting unsupported for ", names[id].name);
			}
			return 0;
		}

		void state(Regulator_id id, bool enable)
		{
			if (enable)
				_enable(id);
			else
				_disable(id);
		}

		bool state(Regulator_id id)
		{
			switch (id) {
			case CLK_USB20:
				return read<Clk_gate_ip_fsys::Usbhost20>();
			default:
				Genode::warning("state request unsupported for ", names[id].name);
			}
			return true;
		}
};


const Genode::uint8_t  Cmu::s_values[] = { 2, 1, 1, 0, 0, 0, 0, 0, 0 };
const Genode::uint16_t Cmu::m_values[] = { 100, 100, 200, 100, 125,
                                           150, 175, 200, 425 };
const Genode::uint8_t  Cmu::p_values[] = { 3, 3, 4, 3, 3, 3, 3, 3, 6 };
const Genode::uint32_t Cmu::Clk_div_cpu0::values[] = { 0x1117710, 0x1127710, 0x1137710,
                                                       0x2147710, 0x2147710, 0x3157720,
                                                       0x4167720};
#endif /* _DRIVERS__PLATFORM__SPEC__ODROID_X2__CMU_H_ */
