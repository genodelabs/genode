/*
 * \brief   MMU initialization for Cortex A8
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

Bootstrap::Platform::Cpu_id Bootstrap::Platform::enable_mmu()
{
	::Board::Global_interrupt_controller gic { };
	::Board::Local_interrupt_controller lic { gic };
	::Board::Cpu::Sctlr::init();
	::Board::Cpu::enable_mmu_and_caches((addr_t)core_pd->table_base);

	return { 0 };
}
