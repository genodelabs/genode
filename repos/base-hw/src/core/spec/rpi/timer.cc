/*
 * \brief   Timer implementation specific to Rpi
 * \author  Norman Feske
 * \author  Stefan Kalkowski
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <timer.h>

using namespace Genode;
using Kernel::time_t;


Timer::Timer()
: Mmio(Platform::mmio_to_virt(Board::SYSTEM_TIMER_MMIO_BASE)) { }


void Timer::start_one_shot(time_t const tics, unsigned const)
{
	write<Cs::M1>(1);
	read<Cs>();
	write<Clo>(0);
	write<Cmp>(read<Clo>() + tics);
}


time_t Timer::tics_to_us(time_t const tics) const {
	return (tics / TICS_PER_MS) * 1000; }


time_t Timer::us_to_tics(time_t const us) const {
	return (us / 1000) * TICS_PER_MS; }


time_t Timer::max_value() { return (Clo::access_t)~0; }


time_t Timer::value(unsigned const)
{
	Cmp::access_t const cmp = read<Cmp>();
	Clo::access_t const clo = read<Clo>();
	return cmp > clo ? cmp - clo : 0;
}
