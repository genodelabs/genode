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

#ifndef _CORE__SPEC__CORTEX_A9__CPU_H_
#define _CORE__SPEC__CORTEX_A9__CPU_H_

/* core includes */
#include <spec/arm_v7/cpu_support.h>
#include <board.h>

namespace Genode { struct Cpu; }

struct Genode::Cpu : Arm_v7_cpu
{
	/**
	 * Clean and invalidate data-cache for virtual region
	 * 'base' - 'base + size'
	 */
	static void clean_invalidate_data_cache_by_virt_region(addr_t const base,
	                                                       size_t const size)
	{
		Arm_cpu::clean_invalidate_data_cache_by_virt_region(base, size);
		Board::l2_cache().clean_invalidate();
	}

	static unsigned executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }
};

#endif /* _CORE__SPEC__CORTEX_A9__CPU_H_ */
