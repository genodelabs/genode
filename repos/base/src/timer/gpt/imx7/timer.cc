/*
 * \brief  Time source for i.MX7 (GPT1)
 * \author Stefan Kalkowski
 * \date   2019-04-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local include */
#include <time_source.h>

using namespace Genode;

enum {
	MMIO_BASE = 0x302d0000,
	MMIO_SIZE = 0x1000,
	IRQ       = 87,
};


Timer::Time_source::Time_source(Env &env)
: Attached_mmio(env, MMIO_BASE, MMIO_SIZE),
  Signalled_time_source(env),
  _timer_irq(env, IRQ) { _initialize(); }
