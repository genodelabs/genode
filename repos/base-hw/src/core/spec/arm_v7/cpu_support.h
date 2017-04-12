/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_V7__CPU_SUPPORT_H_
#define _CORE__SPEC__ARM_V7__CPU_SUPPORT_H_

/* core includes */
#include <spec/arm/cpu_support.h>

namespace Genode { struct Arm_v7_cpu; }


struct Genode::Arm_v7_cpu : Arm_cpu
{
	/**
	 * Wait for the next interrupt as cheap as possible
	 */
	static void wait_for_interrupt() { asm volatile ("wfi"); }

	/**
	 * Write back dirty lines of inner data cache and invalidate all
	 */
	static void clean_invalidate_inner_data_cache();

	/**
	 * Invalidate all lines of the inner data cache
	 */
	static void invalidate_inner_data_cache();
};

#endif /* _CORE__SPEC__ARM_V7__CPU_SUPPORT_H_ */
