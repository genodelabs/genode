/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw core includes */
#include <spec/arm/cortex_a9_cpu.h>
#include <board.h>


void Core::Cpu::cache_clean_invalidate_data_region(addr_t const base,
                                                   size_t const size)
{
	Arm_cpu::cache_clean_invalidate_data_region(base, size);
	Board::l2_cache().clean_invalidate();
}
