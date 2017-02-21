/*
 * \brief  CPU definitions for ARM
 * \author Stefan Kalkowski
 * \date   2017-02-22
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARM__CPU_H_
#define _SRC__BOOTSTRAP__SPEC__ARM__CPU_H_

#include <hw/spec/arm/cpu.h>

namespace Bootstrap { struct Cpu; }

struct Bootstrap::Cpu : Hw::Arm_cpu
{
	struct Sctlr : Hw::Arm_cpu::Sctlr
	{
		static void init()
		{
			/*
			 * disable alignment checks and
			 * set exception vector to 0xffff0000
			 */
			access_t v = read();
			A::set(v, 0);
			V::set(v, 1);
			write(v);
		}
	};

	struct Cpsr : Hw::Arm_cpu::Cpsr
	{
		static void init()
		{
			access_t v = read();
			Psr::F::set(v, 1);
			Psr::A::set(v, 1);
			Psr::M::set(v, Psr::M::SVC);
			Psr::I::set(v, 1);
			write(v);
		}
	};

	enum Errata { ARM_764369 };

	static bool errata(Errata);

	static void wake_up_all_cpus(void * const ip);

	static void enable_mmu_and_caches(Genode::addr_t table);
};

#endif /* _SRC__BOOTSTRAP__SPEC__ARM__CPU_H_ */
