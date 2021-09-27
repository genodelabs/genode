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

/* base-hw Core includes */
#include <spec/cortex_a9/cpu.h>
#include <board.h>


void Genode::Cpu::cache_clean_invalidate_data_region(addr_t const base,
                                                     size_t const size)
{
	Arm_cpu::cache_clean_invalidate_data_region(base, size);
	Board::l2_cache().clean_invalidate();
}
