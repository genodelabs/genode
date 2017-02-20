/*
 * \brief   Specific i.MX53 bootstrap implementations
 * \author  Stefan Kalkowski
 * \date    2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <cpu.h>


void Platform::enable_mmu()
{
	Genode::Cpu::Sctlr::init();

	cpu.enable_mmu_and_caches((addr_t)core_pd->table_base);
}


void Genode::Cpu::translation_added(Genode::addr_t const addr,
                                    Genode::size_t const size) { }
