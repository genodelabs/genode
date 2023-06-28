/*
 * \brief  Time source for i.MX6 (EPIT2)
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

/* local include */
#include <time_source.h>

enum {
	EPIT_2_IRQ       = 89,
	EPIT_2_MMIO_BASE = 0x020d4000,
	EPIT_2_MMIO_SIZE = 0x00004000
};

using namespace Genode;

Timer::Time_source::Time_source(Env &env)
:
	Attached_mmio(env, EPIT_2_MMIO_BASE, EPIT_2_MMIO_SIZE),
	Signalled_time_source(env),
	_timer_irq(env, unsigned(EPIT_2_IRQ))
{
	_timer_irq.sigh(_signal_handler);
	while (read<Cr::Swr>()) ;
}
