/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/timer.h>
#include <board.h>
#include <platform.h>

#include <drivers/timer/util.h>

using namespace Core;
using namespace Kernel;


unsigned Timer::interrupt_id() const { return Board::EPIT_1_IRQ; }


Board::Timer::Timer(unsigned)
:
	Mmio(Platform::mmio_to_virt(Board::EPIT_1_MMIO_BASE))
{
	init();
}


void Board::Timer::init()
{
	/*
	 * Used timer mode:
	 *
	 * - Set CNT to 0xffffffff whenever CR.EN goes from 0 to 1 (CR.EN_MOD = 1).
	 *   This happens only once: at construction time.
	 * - CNT is counting downwards with timer frequency.
	 * - When CNT reaches 0 it rolls over to 0xffffffff (CR.RLD = 0).
	 * - When writing LR, also set CNT to new LR value (CR.IOVW = 1). This
	 *   happens whenever a timeout is programmed.
	 * - Trigger IRQ when CNT == CMPR (CR.OCI_EN = 1). CMPR is always set to
	 *   0xffffffff.
	 */

	reset();

	Cr::access_t cr = read<Cr>();
	Cr::En_mod::set(cr, Cr::En_mod::RELOAD);
	Cr::Oci_en::set(cr, 1);
	Cr::Prescaler::set(cr, Cr::Prescaler::DIVIDE_BY_1);
	Cr::Clk_src::set(cr, Cr::Clk_src::HIGH_FREQ_REF_CLK);
	Cr::Iovw::set(cr, 1);
	write<Cr>(cr);

	write<Cmpr>(0xffffffff);
	write<Cr::En>(1);

	write<Lr>(0xffffffff);
}


void Timer::_start_one_shot(time_t const ticks)
{
	/*
	 * First unset the interrupt flag,
	 * otherwise if the tick is small enough, we loose an interrupt
	 */
	_device.write<Board::Timer::Sr::Ocif>(1);

	/* maximal ticks are guaranteed via _max_value */
	_device.write<Board::Timer::Lr>((uint32_t)(ticks - 1));
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, Board::Timer::TICS_PER_MS); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000UL) * Board::Timer::TICS_PER_MS; }


time_t Timer::_max_value() const {
	return 0xffffffff; }


time_t Timer::_duration() const
{
	using Device = Board::Timer;
	Device::Cnt::access_t const last = (Device::Cnt::access_t) _last_timeout_duration;
	Device::Cnt::access_t const cnt  = _device.read<Device::Cnt>();
	return (_device.read<Device::Sr::Ocif>()) ? _max_value() - _device.read<Device::Cnt>() + last
	                                          : last - cnt;
}
