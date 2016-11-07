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

#ifndef _DRIVERS__PLATFORM__SPEC__ARNDALE__CMU_H_
#define _DRIVERS__PLATFORM__SPEC__ARNDALE__CMU_H_

#include <base/log.h>
#include <regulator/consts.h>
#include <regulator/driver.h>
#include <drivers/board_base.h>
#include <os/attached_mmio.h>

using namespace Regulator;
using Genode::warning;

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

		typedef Pll_lock<0>     Apll_lock;
		typedef Pll_con0<0x100> Apll_con0;

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


		/************************
		 ** CMU CORE registers **
		 ************************/

		typedef Pll_lock<0x4000> Mpll_lock;
		typedef Pll_con0<0x4100> Mpll_con0;

		struct Clk_src_core1 : Register<0x4204, 32>
		{
			struct Mux_mpll_sel : Bitfield<8, 1> { enum { XXTI, MPLL_FOUT_RGT }; };
		};

		struct Clk_gate_ip_acp   : Register<0x8800, 32> { };
		struct Clk_gate_ip_isp0  : Register<0xc800, 32> { };
		struct Clk_gate_ip_isp1  : Register<0xc804, 32> { };
		struct Clk_gate_sclk_isp : Register<0xc900, 32> { };


		/***********************
		 ** CMU TOP registers **
		 ***********************/

		typedef Pll_lock<0x10020> Cpll_lock;
		typedef Pll_lock<0x10030> Epll_lock;
		typedef Pll_lock<0x10040> Vpll_lock;
		typedef Pll_lock<0x10050> Gpll_lock;
		typedef Pll_con0<0x10120> Cpll_con0;
		typedef Pll_con0<0x10130> Epll_con0;
		typedef Pll_con0<0x10140> Vpll_con0;
		typedef Pll_con0<0x10150> Gpll_con0;

		struct Clk_src_top2 : Register<0x10218, 32>
		{
			struct Mux_mpll_user_sel : Bitfield<20, 1> { enum { XXTI, MOUT_MPLL}; };
		};

		struct Clk_src_fsys : Register<0x10244, 32>
		{
			struct Sata_sel     : Bitfield<24, 1> {
				enum { SCLK_MPLL_USER, SCLK_BPLL_USER }; };
			struct Usbdrd30_sel : Bitfield<28, 1> {
				enum { SCLK_MPLL_USER, SCLK_CPLL }; };
		};

		struct Clk_src_mask_fsys : Register<0x10340, 32>
		{
			struct Mmc0_mask     : Bitfield<0,  1> { enum { MASK, UNMASK }; };
			struct Sata_mask     : Bitfield<24, 1> { enum { MASK, UNMASK }; };
			struct Usbdrd30_mask : Bitfield<28, 1> { enum { MASK, UNMASK }; };
		};

		struct Clk_div_fsys0 : Register<0x10548, 32>
		{
			struct Sata_ratio     : Bitfield<20, 4> { };
			struct Usbdrd30_ratio : Bitfield<24, 4> { };
		};

		struct Clk_div_stat_fsys0 : Register<0x10648, 32>
		{
			struct Div_sata     : Bitfield<20, 1> {};
			struct Div_usbdrd30 : Bitfield<24, 1> {};
		};

		struct Clk_gate_ip_gscl  : Register<0x10920, 32> { };
		struct Clk_gate_ip_disp1 : Register<0x10928, 32>
		{
			struct Clk_mixer     : Bitfield<5, 1> { };
			struct Clk_hdmi      : Bitfield<6, 1> { };
		};
		struct Clk_gate_ip_mfc   : Register<0x1092c, 32> { };
		struct Clk_gate_ip_g3d   : Register<0x10930, 32> { };
		struct Clk_gate_ip_gen   : Register<0x10934, 32> { };

		struct Clk_gate_ip_fsys : Register<0x10944, 32>
		{
			struct Pdma0         : Bitfield<1,  1> { };
			struct Pdma1         : Bitfield<2,  1> { };
			struct Sata          : Bitfield<6,  1> { };
			struct Sdmmc0        : Bitfield<12, 1> { };
			struct Usbhost20     : Bitfield<18, 1> { };
			struct Usbdrd30      : Bitfield<19, 1> { };
			struct Sata_phy_ctrl : Bitfield<24, 1> { };
			struct Sata_phy_i2c  : Bitfield<25, 1> { };
		};

		struct Clk_src_disp1_0 : Register<0x1022c, 32>
		{
			struct Hdmi_sel : Bitfield<20, 1> { };
		};

		struct Clk_src_mask_disp1_0 : Register<0x1032c, 32>
		{
			struct Hdmi_mask : Bitfield<20, 1> { };
		};

		struct Clk_gate_ip_peric : Register<0x10950, 32>
		{
			struct Clk_uart2   : Bitfield<2,  1> { };
			struct Clk_i2chdmi : Bitfield<14, 1> { };
			struct Clk_pwm     : Bitfield<24, 1> { };
		};

		struct Clk_gate_block : Register<0x10980, 32>
		{
			struct Clk_disp1 : Bitfield<5, 1> { };
			struct Clk_gen   : Bitfield<2, 1> { };
		};


		/*************************
		 ** CMU CDREX registers **
		 *************************/

		typedef Pll_lock<0x20010> Bpll_lock;
		typedef Pll_con0<0x20110> Bpll_con0;

		struct Pll_div2_sel : Register<0x20a24, 32>
		{
			struct Mpll_fout_sel : Bitfield<4, 1> {
				enum { MPLL_FOUT_HALF, MPLL_FOUT }; };
		};


		/*******************
		 ** CPU functions **
		 *******************/

		Cpu_clock_freq _cpu_freq;

		void _cpu_clk_freq(unsigned long level)
		{
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
			case CPU_FREQ_1600:
				freq = 7;
				break;
			case CPU_FREQ_1700:
				freq = 8;
				break;
			default:
				warning("Unsupported CPU frequency level ", level);
				warning("Supported values are 200, 400, 600, 800 MHz");
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
			write<Clk_src_cpu::Mux_cpu_sel>(Clk_src_cpu::Mux_cpu_sel::SCLK_MPLL);
			while (read<Clk_mux_stat_cpu::Cpu_sel>()
			       != Clk_mux_stat_cpu::Cpu_sel::SCLK_MPLL) ;

			/* set lock time */
			unsigned pdiv = p_values[freq];
			write<Apll_lock::Pll_locktime>(Apll_lock::max_lock_time(pdiv));

			/* change P, M, S values of APLL */
			write<Apll_con0::P>(p_values[freq]);
			write<Apll_con0::M>(m_values[freq]);
			write<Apll_con0::S>(s_values[freq]);

			while (!read<Apll_con0::Locked>()) ;

			/* change reference clock back to APLL */
			write<Clk_src_cpu::Mux_cpu_sel>(Clk_src_cpu::Mux_cpu_sel::MOUT_APLL);
			while (read<Clk_mux_stat_cpu::Cpu_sel>()
			       != Clk_mux_stat_cpu::Cpu_sel::MOUT_APLL) ;

			_cpu_freq = static_cast<Cpu_clock_freq>(level);
		}


		/**********************
		 ** Device functions **
		 **********************/

		void _hdmi_enable()
		{
			write<Clk_gate_ip_peric::Clk_i2chdmi>(1);
			Clk_gate_ip_disp1::access_t gd1 = read<Clk_gate_ip_disp1>();
			Clk_gate_ip_disp1::Clk_mixer::set(gd1, 1);
			Clk_gate_ip_disp1::Clk_hdmi::set(gd1, 1);
			write<Clk_gate_ip_disp1>(gd1);
			write<Clk_gate_block::Clk_disp1>(1);
			write<Clk_src_mask_disp1_0::Hdmi_mask>(1);
			write<Clk_src_disp1_0::Hdmi_sel>(1);
		}

		void _sata_enable()
		{
			/* enable I2C for SATA */
			write<Clk_gate_ip_fsys::Sata_phy_i2c>(1);

			/**
			 * set SATA clock to 66 MHz (nothing else supported)
			 * assuming 800 MHz from sclk_mpll_user, formula: sclk / (divider + 1)
			 */
			write<Clk_div_fsys0::Sata_ratio>(11); /*  */
			while (read<Clk_div_stat_fsys0::Div_sata>()) ;

			/* enable SATA and SATA Phy */
			write<Clk_gate_ip_fsys::Sata>(1);
			write<Clk_gate_ip_fsys::Sata_phy_ctrl>(1);
			write<Clk_src_mask_fsys::Sata_mask>(1);
		}

		void _usb30_enable()
		{
			/**
			 * set USBDRD30 clock to 66 MHz
			 * assuming 800 MHz from sclk_mpll_user, formula: sclk / (divider + 1)
			 */
			write<Clk_div_fsys0::Usbdrd30_ratio>(11);
			while (read<Clk_div_stat_fsys0::Div_usbdrd30>()) ;

			/* enable USBDRD30 clock */
			write<Clk_gate_ip_fsys::Usbdrd30>(1);
			write<Clk_src_mask_fsys::Usbdrd30_mask>(1);
		}

		void _enable(Regulator_id id)
		{
			switch (id) {
			case CLK_SATA:
				_sata_enable();
				break;
			case CLK_HDMI:
				_hdmi_enable();
				break;
			case CLK_USB30:
				_usb30_enable();
				break;
			case CLK_USB20:
				return write<Clk_gate_ip_fsys::Usbhost20>(1);
			case CLK_MMC0:
				write<Clk_gate_ip_fsys::Sdmmc0>(1);
				write<Clk_src_mask_fsys::Mmc0_mask>(1);
				break;
			default:
				warning("Unsupported for ", names[id].name);
			}
		}

		void _disable(Regulator_id id)
		{
			switch (id) {
			case CLK_SATA:
				write<Clk_gate_ip_fsys::Sata_phy_i2c>(0);
				write<Clk_gate_ip_fsys::Sata>(0);
				write<Clk_gate_ip_fsys::Sata_phy_ctrl>(0);
				write<Clk_src_mask_fsys::Sata_mask>(0);
				break;
			case CLK_USB30:
				write<Clk_gate_ip_fsys::Usbdrd30>(0);
				write<Clk_src_mask_fsys::Usbdrd30_mask>(0);
				break;
			case CLK_USB20:
				return write<Clk_gate_ip_fsys::Usbhost20>(0);
			case CLK_MMC0:
				write<Clk_gate_ip_fsys::Sdmmc0>(0);
				write<Clk_src_mask_fsys::Mmc0_mask>(0);
				break;
			default:
				warning("Unsupported for ", names[id].name);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Cmu()
		: Genode::Attached_mmio(Genode::Board_base::CMU_MMIO_BASE,
		                        Genode::Board_base::CMU_MMIO_SIZE),
		  _cpu_freq(CPU_FREQ_1600)
		{
			/**
			 * Close certain clock gates by default (~ 0.7 Watt reduction)
			 */
			write<Clk_gate_ip_acp>(0);
			write<Clk_gate_ip_isp0>(0);
			write<Clk_gate_ip_isp1>(0);
			write<Clk_gate_sclk_isp>(0);
			write<Clk_gate_ip_gscl>(0);
			write<Clk_gate_ip_disp1>(0);
			write<Clk_gate_ip_mfc>(0);
			write<Clk_gate_ip_g3d>(0);
			write<Clk_gate_ip_gen>(0);
			write<Clk_gate_ip_fsys>(0);
			write<Clk_gate_ip_peric>(Clk_gate_ip_peric::Clk_uart2::bits(1) |
			                         Clk_gate_ip_peric::Clk_pwm::bits(1));
			write<Clk_gate_block>(Clk_gate_block::Clk_gen::bits(1));


			/**
			 * Set default CPU frequency
			 */
			_cpu_clk_freq(_cpu_freq);

			/**
			 * Hard wiring of certain reference clocks
			 */
			write<Pll_div2_sel::Mpll_fout_sel>(Pll_div2_sel::Mpll_fout_sel::MPLL_FOUT_HALF);
			write<Clk_src_core1::Mux_mpll_sel>(Clk_src_core1::Mux_mpll_sel::MPLL_FOUT_RGT);
			write<Clk_src_top2::Mux_mpll_user_sel>(Clk_src_top2::Mux_mpll_user_sel::MOUT_MPLL);
			write<Clk_src_fsys::Sata_sel>(Clk_src_fsys::Sata_sel::SCLK_MPLL_USER);
			write<Clk_src_fsys::Usbdrd30_sel>(Clk_src_fsys::Usbdrd30_sel::SCLK_MPLL_USER);
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
				warning("Unsupported for ", names[id].name);
			}
		}

		unsigned long level(Regulator_id id)
		{
			switch (id) {
			case CLK_CPU:
				return _cpu_freq;
			case CLK_USB30:
			case CLK_SATA:
				return 66666666; /* 66 MHz */
			default:
				warning("Unsupported for ", names[id].name);
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
			case CLK_SATA:
				return read<Clk_gate_ip_fsys::Sata>() &&
				       read<Clk_gate_ip_fsys::Sata_phy_ctrl>() &&
				       read<Clk_src_mask_fsys::Sata_mask>();
			case CLK_USB30:
				return read<Clk_gate_ip_fsys::Usbdrd30>() &&
				       read<Clk_src_mask_fsys::Usbdrd30_mask>();
			case CLK_USB20:
				return read<Clk_gate_ip_fsys::Usbhost20>();
			case CLK_MMC0:
				return read<Clk_gate_ip_fsys::Sdmmc0>() &&
				       read<Clk_src_mask_fsys::Mmc0_mask>();
			default:
				warning("Unsupported for ", names[id].name);
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
                                                       0x4167720, 0x4177730, 0x5377730 };
#endif /* _DRIVERS__PLATFORM__SPEC__ARNDALE__CMU_H_ */
