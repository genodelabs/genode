/**
 * \brief  Platform-level interrupt controller layout (PLIC)
 * \author Sebastian Sumpf
 * \date   2021-03-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__RISCV_QEMU__PLIC_H_
#define _CORE__SPEC__RISCV_QEMU__PLIC_H_

namespace Board { class Plic; }

struct Board::Plic : Genode::Mmio
{
		enum { NR_OF_IRQ = 32 };

		struct Enable : Register_array<0x80, 32, 32, 1> { };
		struct Id     : Register<0x1ff004, 32> { };

		Plic(Genode::addr_t const base)
		:
			Mmio(base) { }

		void enable(unsigned value, unsigned irq)
		{
			write<Enable>(value, irq);
		}

		void el(unsigned, unsigned) { }
};

#endif /* _CORE__SPEC__RISCV_QEMU__PLIC_H_ */
