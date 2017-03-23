/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \author Martin stein
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core include */
#include <kernel/timer.h>
#include <board.h>
#include <platform.h>

using namespace Genode;
using namespace Kernel;


unsigned Timer::interrupt_id() const
{
	switch (_driver.cpu_id) {
	case 0:  return Board::MCT_IRQ_L0;
	case 1:  return Board::MCT_IRQ_L1;
	default: return 0;
	}
}


Timer_driver::Timer_driver(unsigned cpu_id)
:
	Mmio(Platform::mmio_to_virt(Board::MCT_MMIO_BASE)),
	ticks_per_ms(calc_ticks_per_ms(Board::MCT_CLOCK)),
	cpu_id(cpu_id)
{
	Mct_cfg::access_t mct_cfg = 0;
	Mct_cfg::Prescaler::set(mct_cfg, PRESCALER);
	Mct_cfg::Div_mux::set(mct_cfg, DIV_MUX);
	write<Mct_cfg>(mct_cfg);
	write<L0_int_enb>(L0_int_enb::Frceie::bits(1));
	write<L1_int_enb>(L1_int_enb::Frceie::bits(1));
}


void Timer::_start_one_shot(time_t const ticks)
{
	switch (_driver.cpu_id) {
	case 0:
		_driver.write<Driver::L0_int_cstat::Frcnt>(1);
		_driver.run_0(0);
		_driver.acked_write<Driver::L0_frcntb, Driver::L0_wstat::Frcntb>(ticks);
		_driver.run_0(1);
		return;
	case 1:
		_driver.write<Driver::L1_int_cstat::Frcnt>(1);
		_driver.run_1(0);
		_driver.acked_write<Driver::L1_frcntb, Driver::L1_wstat::Frcntb>(ticks);
		_driver.run_1(1);
		return;
	default: return;
	}
}


time_t Timer::_value()
{
	switch (_driver.cpu_id) {
	case 0: return _driver.read<Driver::L0_int_cstat::Frcnt>() ? 0 : _driver.read<Driver::L0_frcnto>();
	case 1: return _driver.read<Driver::L1_int_cstat::Frcnt>() ? 0 : _driver.read<Driver::L1_frcnto>();
	default: return 0;
	}
}


time_t Timer::_ticks_to_us(time_t const ticks) const {
	return (ticks / _driver.ticks_per_ms) * 1000; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * _driver.ticks_per_ms; }


time_t Timer::_max_value() const {
	return (Driver::L0_frcnto::access_t)~0; }
