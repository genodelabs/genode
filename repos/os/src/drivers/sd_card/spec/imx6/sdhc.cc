/*
 * \brief  Secured Digital Host Controller
 * \author Martin Stein
 * \date   2016-12-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <sdhc.h>

using namespace Sd_card;
using namespace Genode;


void Sdhc::_stop_transmission_finish_xfertyp(Xfertyp::access_t &xfertyp)
{
	Mixctrl::access_t mixctrl = read<Mixctrl>();
	Mixctrl::Dmaen::set(mixctrl, 1);
	Mixctrl::Bcen::set(mixctrl, 1);
	Mixctrl::Ac12en::set(mixctrl, 0);
	Mixctrl::Ddren::set(mixctrl, 0);
	Mixctrl::Dtdsel::set(mixctrl, Mixctrl::Dtdsel::READ);
	Mixctrl::Msbsel::set(mixctrl, 1);
	Mixctrl::Nibblepos::set(mixctrl, 0);
	Mixctrl::Ac23en::set(mixctrl, 0);
	write<Mixctrl>(mixctrl);
}


int Sdhc::_wait_for_cmd_complete_mb_finish(bool const reading)
{
	/* we can't use the "Auto Command 12" feature as it does not work */
	return _stop_transmission() ? -1 : 0;
}


bool Sdhc::_issue_cmd_finish_xfertyp(Xfertyp::access_t &,
                                     bool const         transfer,
                                     bool const         multiblock,
                                     bool const         reading)
{
	Mixctrl::access_t mixctrl = read<Mixctrl>();
	Mixctrl::Dmaen    ::set(mixctrl, transfer && multiblock && _use_dma);
	Mixctrl::Bcen     ::set(mixctrl, transfer);
	Mixctrl::Ac12en   ::set(mixctrl, 0);
	Mixctrl::Msbsel   ::set(mixctrl, transfer);
	Mixctrl::Ddren    ::set(mixctrl, 0);
	Mixctrl::Nibblepos::set(mixctrl, 0);
	Mixctrl::Ac23en   ::set(mixctrl, 0);
	Mixctrl::Dtdsel   ::set(mixctrl, reading ? Mixctrl::Dtdsel::READ :
	                                           Mixctrl::Dtdsel::WRITE);

	if (_wait_for_cmd_allowed()) {
		return false; }

	write<Mixctrl>(mixctrl);
	return true;
}


bool Sdhc::_supported_host_version(Hostver::access_t hostver)
{
	return Hostver::Vvn::get(hostver) == 0 &&
	       Hostver::Svn::get(hostver) == 3;
}


void Sdhc::_watermark_level(Wml::access_t &wml)
{
	Wml::Wr_wml::set(wml, 64);
	Wml::Wr_brst_len::set(wml, 16);
}


void Sdhc::_reset_amendments()
{
	/* the USDHC doesn't reset the Mixer Control register automatically */
	Mixctrl::access_t mixctrl = read<Mixctrl>();
	Mixctrl::Dmaen::set(mixctrl, 0);
	Mixctrl::Bcen::set(mixctrl, 0);
	Mixctrl::Ac12en::set(mixctrl, 0);
	Mixctrl::Ddren::set(mixctrl, 0);
	Mixctrl::Dtdsel::set(mixctrl, 0);
	Mixctrl::Msbsel::set(mixctrl, 0);
	Mixctrl::Nibblepos::set(mixctrl, 0);
	Mixctrl::Ac23en::set(mixctrl, 0);
	Mixctrl::Always_ones::set(mixctrl, 1);
	write<Mixctrl>(mixctrl);
}


void Sdhc::_clock_finish(Clock clock)
{
	switch (clock) {
	case CLOCK_INITIAL:
		write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_13);
		_enable_clock(CLOCK_DIV_512);
		break;
	case CLOCK_OPERATIONAL:
		write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_28);
		write<Sysctl::Ipp_rst_n>(0);
		_enable_clock(CLOCK_DIV_4);
		break;
	}
}


void Sdhc::_disable_clock_preparation() { write<Vendspec::Frc_sdclk_on>(0); }

void Sdhc::_enable_clock_finish() { write<Vendspec::Frc_sdclk_on>(0); }
