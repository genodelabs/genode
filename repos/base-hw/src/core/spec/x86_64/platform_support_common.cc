/*
 * \brief   Platform implementations specific for x86
 * \author  Norman Feske
 * \author  Reto Buerki
 * \date    2013-04-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <cpu.h>

using namespace Genode;

void Platform::_init_io_port_alloc()
{
	_io_port_alloc.add_range(0, 0x10000);
}


long Platform::irq(long const user_irq)
{
	/* remap IRQ requests to fit I/O APIC configuration */
	if (user_irq) return user_irq + Board::VECTOR_REMAP_BASE;
	return Board::TIMER_VECTOR_USER;
}
