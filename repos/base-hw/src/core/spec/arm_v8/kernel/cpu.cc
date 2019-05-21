/*
 * \brief   Kernel cpu driver implementations specific to ARMv8
 * \author  Stefan Kalkowski
 * \date    2019-05-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>

void Kernel::Cpu::_arch_init()
{
	/* enable timer interrupt */
	_pic.unmask(_timer.interrupt_id(), id());
}
