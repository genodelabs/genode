/*
 * \brief  x86_64 constants
 * \author Reto Buerki
 * \date   2015-03-18
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__BOARD_H_
#define _CORE__INCLUDE__SPEC__X86_64__BOARD_H_

namespace Genode
{
	struct Board
	{
		enum {
			MMIO_LAPIC_BASE  = 0xfee00000,
			MMIO_LAPIC_SIZE  = 0x1000,
			MMIO_IOAPIC_BASE = 0xfec00000,
			MMIO_IOAPIC_SIZE = 0x1000,

			VECTOR_REMAP_BASE   = 48,
			TIMER_VECTOR_KERNEL = 32,
			TIMER_VECTOR_USER   = 50,
			ISA_IRQ_END         = 15,
		};

		void init() { }
	};
}

#endif /* _CORE__INCLUDE__SPEC__X86_64__BOARD_H_ */
