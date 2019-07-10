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

struct Bootstrap::Actlr : Board::Cpu::Actlr
{
	struct Fw                 : Bitfield<0, 1> { };
	struct L2_prefetch_enable : Bitfield<1, 1> { };
	struct L1_prefetch_enable : Bitfield<2, 1> { };
	struct Write_full_line    : Bitfield<3, 1> { };
	struct Smp                : Bitfield<6, 1> { };

	static void enable_smp()
	{
		auto v = read();
		Fw::set(v, 1);
		L1_prefetch_enable::set(v, 1);
		L2_prefetch_enable::set(v, 1);
		Write_full_line::set(v, 1);
		Smp::set(v, 1);
		write(v);
	}

	static void disable_smp()
	{
		auto v = read();
		Smp::set(v, 0);
		write(v);
	}
};

#endif /* _SRC__BOOTSTRAP__SPEC__ARM__CORTEX_A9_ACTLR_H_ */
