/*
 * \brief  CPU core implementation
 * \author Sebastian Sumpf
 * \date   2016-02-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <kernel/interface.h>

/* Core includes */
#include <cpu.h>
#include <machine_call.h>

void Genode::Cpu::translation_added(addr_t const addr, size_t const size)
{
	if (Machine::is_user_mode())
		Kernel::update_data_region(addr, size);
	else Genode::Cpu::sfence();
}
