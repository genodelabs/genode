/*
 * \brief  Definitions common to all Cortex A15 CPUs
 * \author Stefan Kalkowski
 * \date   2017-02-23
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__CORTEX_A15_H_
#define _SRC__LIB__HW__SPEC__ARM__CORTEX_A15_H_

#include <base/stdint.h>

namespace Hw { template <Genode::addr_t> struct Cortex_a15_mmio; }

template <typename Genode::addr_t BASE>
struct Hw::Cortex_a15_mmio
{
	enum {
		IRQ_CONTROLLER_DISTR_BASE = BASE + 0x1000,
		IRQ_CONTROLLER_DISTR_SIZE = 0x1000,
		IRQ_CONTROLLER_CPU_BASE   = BASE + 0x2000,
		IRQ_CONTROLLER_CPU_SIZE   = 0x2000,
	};
};

#endif /* _SRC__LIB__HW__SPEC__ARM__CORTEX_A15_H_ */
