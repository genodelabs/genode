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

#include <drivers/timer/util.h>

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
	local(Platform::mmio_to_virt(Board::MCT_MMIO_BASE)
	      + (cpu_id ? L1 : L0)),
	ticks_per_ms(calc_ticks_per_ms(Board::MCT_CLOCK)),
	cpu_id(cpu_id)
{
	static unsigned initialized = 0;
	if (initialized++) return;

	Mct_cfg::access_t mct_cfg = 0;
	Mct_cfg::Prescaler::set(mct_cfg, PRESCALER);
	Mct_cfg::Div_mux::set(mct_cfg, DIV_MUX);
	write<Mct_cfg>(mct_cfg);
}


Timer_driver::Local::Local(Genode::addr_t base)
: Mmio(base)
{
	write<Int_enb>(Int_enb::Frceie::bits(1));

	acked_write<Tcntb, Wstat::Tcntb>(0xffffffff);
	acked_write<Frcntb, Wstat::Frcntb>(0xffffffff);

	Tcon::access_t tcon = 0;
	Tcon::Frc_start::set(tcon, 1);
	Tcon::Timer_start::set(tcon, 1);
	acked_write<Tcon, Wstat::Tcon>(tcon);
}


void Timer::_start_one_shot(time_t const ticks)
{
	_driver.local.cnt = _driver.local.read<Driver::Local::Tcnto>();
	_driver.local.write<Driver::Local::Int_cstat::Frccnt>(1);
	_driver.local.acked_write<Driver::Local::Frcntb,
	                          Driver::Local::Wstat::Frcntb>(ticks);
}


time_t Timer::_duration() const
{
	unsigned long ret =  _driver.local.cnt - _driver.local.read<Driver::Local::Tcnto>();
	return ret;
}



time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, _driver.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * _driver.ticks_per_ms; }


time_t Timer::_max_value() const {
	return 0xffffffff; }
