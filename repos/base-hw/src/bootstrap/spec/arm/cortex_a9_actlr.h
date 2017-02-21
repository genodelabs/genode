/*
 * \brief   Cortex A9 specific ACTLR register settings
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARM__CORTEX_A9_ACTLR_H_
#define _SRC__BOOTSTRAP__SPEC__ARM__CORTEX_A9_ACTLR_H_

#include <spec/arm/cpu.h>

namespace Bootstrap { struct Actlr; }

struct Bootstrap::Actlr : Bootstrap::Cpu::Actlr
{
	struct Smp : Bitfield<6, 1> { };

	static void enable_smp()
	{
		auto v = read();
		Smp::set(v, 1);
		write(v);
	}
};

#endif /* _SRC__BOOTSTRAP__SPEC__ARM__CORTEX_A9_ACTLR_H_ */
