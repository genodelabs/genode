/*
 * \brief   Kernel cpu context specific implementation
 * \author  Stefan Kalkowski
 * \date    2015-02-11
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>

void Kernel::Cpu_context::_init(size_t const stack_size, addr_t const table)
{
	r12           = stack_size;
	cpu_exception = Genode::Cpu::Ttbr0::init(table);
	protection_domain(0);
	translation_table(table);
	sctlr = 0x00003805; // FIXME
	ttbrc = 0x80002500; // FIXME
	mair0 = 0x04ff0444; // FIXME
}
