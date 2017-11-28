/*
 * \brief  Time source for Wandboard Quad i.MX6
 * \author Norman Feske
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \author Alexander Boettcher
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base include */
#include <drivers/defs/wand_quad.h>

/* local include */
#include <time_source.h>

using namespace Genode;

Timer::Time_source::Time_source(Env &env)
:
	Attached_mmio(env, Wand_quad::EPIT_2_MMIO_BASE, Wand_quad::EPIT_2_MMIO_SIZE),
	Signalled_time_source(env),
	_timer_irq(env, Wand_quad::EPIT_2_IRQ)
{
	_timer_irq.sigh(_signal_handler);
	while (read<Cr::Swr>()) ;
}
