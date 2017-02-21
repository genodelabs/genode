/*
 * \brief  Definitions common to all x86_64 CPUs
 * \author Stefan Kalkowski
 * \date   2017-04-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__X86_64__X86_64_H_
#define _SRC__LIB__HW__SPEC__X86_64__X86_64_H_

#include <base/stdint.h>

namespace Hw { struct Cpu_memory_map; }


struct Hw::Cpu_memory_map
{
	enum {
		MMIO_LAPIC_BASE  = 0xfee00000,
		MMIO_LAPIC_SIZE  = 0x1000,
		MMIO_IOAPIC_BASE = 0xfec00000,
		MMIO_IOAPIC_SIZE = 0x1000,
	};
};

#endif /* _SRC__LIB__HW__SPEC__X86_64__X86_64_H_ */
