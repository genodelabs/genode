/*
 * \brief   Kernel cpu driver implementations specific to ARM
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/perf_counter.h>

void Kernel::Cpu::_arch_init()
{
	/* enable performance counter */
	perf_counter()->enable();

	/* enable timer interrupt */
	_pic.unmask(_timer.interrupt_id(), id());
}
