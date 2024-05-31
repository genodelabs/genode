/*
 * \brief  Secured Digital Host Controller
 * \author Martin Stein
 * \date   2016-12-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <driver.h>

using namespace Sd_card;
using namespace Genode;


void Driver::_stop_transmission_finish_xfertyp(Xfertyp::access_t &)
{
	Mixctrl::access_t mixctrl = Mmio::read<Mixctrl>();
	Mixctrl::Dmaen::set(mixctrl, 1);
	Mixctrl::Bcen::set(mixctrl, 1);
	Mixctrl::Ac12en::set(mixctrl, 0);
	Mixctrl::Ddren::set(mixctrl, 0);
	Mixctrl::Dtdsel::set(mixctrl, Mixctrl::Dtdsel::READ);
	Mixctrl::Msbsel::set(mixctrl, 1);
	Mixctrl::Nibblepos::set(mixctrl, 0);
	Mixctrl::Ac23en::set(mixctrl, 0);
	Mmio::write<Mixctrl>(mixctrl);
}


int Driver::_wait_for_cmd_complete_mb_finish(bool)
{
	/* we can't use the "Auto Command 12" feature as it does not work */
	return _stop_transmission() ? -1 : 0;
}


bool Driver::_issue_cmd_finish_xfertyp(Xfertyp::access_t &,
                                     bool const         transfer,
                                     bool const         multiblock,
                                     bool const         reading)
{
	Mixctrl::access_t mixctrl = Mmio::read<Mixctrl>();
	Mixctrl::Dmaen    ::set(mixctrl, transfer && multiblock);
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

	Mmio::write<Mixctrl>(mixctrl);
	return true;
}


bool Driver::_supported_host_version(Hostver::access_t)
{
	/*
	 * on i.MX6 there exist board-specific (tested) drivers only,
	 * therefore we do not need to differentiate in between controller versions
	 */
	return true;
}


void Driver::_watermark_level(Wml::access_t &wml)
{
	Wml::Wr_wml::set(wml, 64);
	Wml::Wr_brst_len::set(wml, 16);
}


void Driver::_reset_amendments()
{
	/* the USDHC doesn't reset the Mixer Control register automatically */
	Mixctrl::access_t mixctrl = Mmio::read<Mixctrl>();
	Mixctrl::Dmaen::set(mixctrl, 0);
	Mixctrl::Bcen::set(mixctrl, 0);
	Mixctrl::Ac12en::set(mixctrl, 0);
	Mixctrl::Ddren::set(mixctrl, 0);
	Mixctrl::Dtdsel::set(mixctrl, 0);
	Mixctrl::Msbsel::set(mixctrl, 0);
	Mixctrl::Nibblepos::set(mixctrl, 0);
	Mixctrl::Ac23en::set(mixctrl, 0);
	Mixctrl::Always_ones::set(mixctrl, 1);
	Mmio::write<Mixctrl>(mixctrl);
}


void Driver::_clock_finish(Clock clock)
{
	switch (clock) {
	case CLOCK_INITIAL:
		Mmio::write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_13);
		_enable_clock(CLOCK_DIV_512);
		break;
	case CLOCK_OPERATIONAL:
		Mmio::write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_28);
		Mmio::write<Sysctl::Ipp_rst_n>(0);
		_enable_clock(CLOCK_DIV_4);
		break;
	}
}


void Driver::_disable_clock_preparation() {
	Mmio::write<Vendspec::Frc_sdclk_on>(0); }

void Driver::_enable_clock_finish() { Mmio::write<Vendspec::Frc_sdclk_on>(0); }
