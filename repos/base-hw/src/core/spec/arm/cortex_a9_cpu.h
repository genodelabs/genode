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

/* base-hw core includes */
#include <spec/arm_v7/cpu_support.h>
#include <spec/arm/cortex_a9_page_table.h>

namespace Core { struct Cpu; }


struct Core::Cpu : Arm_v7_cpu
{
	/**
	 * Clean and invalidate data-cache for virtual region
	 * 'base' - 'base + size'
	 */
	static void cache_clean_invalidate_data_region(addr_t const base,
	                                               size_t const size);

	static unsigned executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }
};

#endif /* _CORE__SPEC__CORTEX_A9__CPU_H_ */
