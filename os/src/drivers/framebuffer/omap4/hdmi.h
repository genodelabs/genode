/*
 * \brief  HDMI subsystem registers
 * \author Norman Feske
 * \date   2012-06-11
 */

#ifndef _HDMI_H_
#define _HDMI_H_

/* local includes */
#include <mmio.h>

struct Hdmi : Mmio
{
	struct Pwr_ctrl : Register<0x40, 32>
	{
		enum Pll_cmd_type { ALL_OFF          = 0,
		                    BOTH_ON_ALL_CLKS = 2, };

		struct Pll_cmd    : Bitfield<2, 2> { };
		struct Pll_status : Bitfield<0, 2> { };

		enum Phy_cmd_type { LDOON = 1,
		                    TXON = 2 };

		struct Phy_cmd    : Bitfield<6, 2> { };
		struct Phy_status : Bitfield<4, 2> { };

	};

	struct Video_cfg : Register<0x50, 32>
	{
		struct Start : Bitfield<31, 1> { };

		struct Packing_mode : Bitfield<8, 3>
		{
			enum { PACK_24B = 1 };
		};

		struct Vsp         : Bitfield<7, 1> { };
		struct Hsp         : Bitfield<6, 1> { };
		struct Interlacing : Bitfield<3, 1> { };
		struct Tm          : Bitfield<0, 2> { };
	};

	struct Video_size : Register<0x60, 32>
	{
		struct X : Bitfield<0, 16>  { };
		struct Y : Bitfield<16, 16> { };
	};
	
	struct Video_timing_h : Register<0x68, 32>
	{
		struct Bp : Bitfield<20, 12> { };
		struct Fp : Bitfield<8, 12>  { };
		struct Sw : Bitfield<0, 8>   { };
	};

	struct Video_timing_v : Register<0x6c, 32>
	{
		struct Bp : Bitfield<20, 12> { };
		struct Fp : Bitfield<8, 12>  { };
		struct Sw : Bitfield<0, 8>   { };
	};

	/**
	 * \return true on success
	 */
	bool issue_pwr_pll_command(Pwr_ctrl::Pll_cmd_type cmd, Delayer &delayer)
	{
		write<Pwr_ctrl::Pll_cmd>(cmd);

		return wait_for<Pwr_ctrl::Pll_status>(cmd, delayer);
	}

	bool issue_pwr_phy_command(Pwr_ctrl::Phy_cmd_type cmd, Delayer &delayer)
	{
		write<Pwr_ctrl::Phy_cmd>(cmd);

		return wait_for<Pwr_ctrl::Phy_status>(cmd, delayer);
	}

	struct Pll_control : Register<0x200, 32>
	{
		struct Mode : Bitfield<0, 1>
		{
			enum { MANUAL = 0 };
		};

		struct Reset : Bitfield<3, 1> { };
	};

	struct Pll_status : Register<0x204, 32>
	{
		struct Reset_done : Bitfield<0, 1> { };
		struct Pll_locked : Bitfield<1, 1> { };
	};

	bool wait_until_pll_locked(Delayer &delayer)
	{
		return wait_for<Pll_status::Pll_locked>(1, delayer);
	};

	struct Pll_go : Register<0x208, 32>
	{
		struct Go : Bitfield<0, 1> { };
	};

	bool pll_go(Delayer &delayer)
	{
		write<Pll_go::Go>(1);

		/* wait for PLL_GO bit change and the PLL reaching locked state */
		return wait_for<Pll_go::Go>(1, delayer)
		    && wait_until_pll_locked(delayer);
	}

	struct Cfg1 : Register<0x20c, 32>
	{
		struct Regm : Bitfield<9, 12> { };
		struct Regn : Bitfield<1, 8>  { };
	};

	struct Cfg2 : Register<0x210, 32>
	{
		struct Highfreq_div_by_2 : Bitfield<12, 1> { };
		struct Refen             : Bitfield<13, 1> { };
		struct Clkinen           : Bitfield<14, 1> { };
		struct Refsel            : Bitfield<21, 2> { };
		struct Freq_divider      : Bitfield<1, 3>  { };
	};

	struct Cfg4 : Register<0x220, 32>
	{
		struct Regm2 : Bitfield<18, 7> { };
		struct Regmf : Bitfield<0, 18> { };
	};

	bool reset_pll(Delayer &delayer)
	{
		write<Pll_control::Reset>(0);

		return wait_for<Pll_status::Reset_done>(1, delayer);
	};

	struct Txphy_tx_ctrl : Register<0x300, 32>
	{
		struct Freqout : Bitfield<30, 2> { };
	};

	struct Txphy_digital_ctrl : Register<0x304, 32> { };

	Hdmi(Genode::addr_t const mmio_base) : Mmio(mmio_base) { }
};

#endif /* _HDMI_H_ */
