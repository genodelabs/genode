/*
 * \brief  Timer driver for core
 * \author Martin stein
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <timer.h>

Genode::Timer::Timer()
: Mmio(Platform::mmio_to_virt(Board::MCT_MMIO_BASE)),
  _tics_per_ms(_calc_tics_per_ms(Board::MCT_CLOCK))
{
	Mct_cfg::access_t mct_cfg = 0;
	Mct_cfg::Prescaler::set(mct_cfg, PRESCALER);
	Mct_cfg::Div_mux::set(mct_cfg, DIV_MUX);
	write<Mct_cfg>(mct_cfg);
	write<L0_int_enb>(L0_int_enb::Frceie::bits(1));
	write<L1_int_enb>(L1_int_enb::Frceie::bits(1));
}
