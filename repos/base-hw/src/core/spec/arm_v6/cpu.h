/*
 * \brief  CPU driver for core
 * \author Norman Feske
 * \author Martin stein
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_V6__CPU_H_
#define _CORE__SPEC__ARM_V6__CPU_H_

/* core includes */
#include <spec/arm/cpu_support.h>

namespace Genode { struct Cpu; }

struct Genode::Cpu : Arm_cpu
{
	static inline void synchronization_barrier() {}

	static inline size_t data_cache_line_size()
	{
		struct Ctr : Genode::Register<32> {
			struct D_min_line : Bitfield<12,2> {};
		};

		static size_t data_cache_line_size = 0;

		if (!data_cache_line_size) {
			data_cache_line_size =
				(1 << (Ctr::D_min_line::get(Arm_cpu::Ctr::read())+1)) * sizeof(addr_t);
		}

		return data_cache_line_size;
	}

	static inline size_t instruction_cache_line_size()
	{
		struct Ctr : Genode::Register<32> {
			struct I_min_line : Bitfield<0,2> {};
		};

		static size_t instruction_cache_line_size = 0;

		if (!instruction_cache_line_size) {
			instruction_cache_line_size =
				(1 << (Ctr::I_min_line::get(Arm_cpu::Ctr::read())+1)) * sizeof(addr_t);
		}

		return instruction_cache_line_size;
	}
};

#endif /* _CORE__SPEC__ARM_V6__CPU_H_ */
