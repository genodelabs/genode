/*
 * \brief  Interrupt Descriptor Table (IDT)
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__IDT_H_
#define _CORE__INCLUDE__SPEC__X86_64__IDT_H_

#include <base/stdint.h>

namespace Genode
{
	/**
	 * Interrupt Descriptor Table (IDT)
	 * See Intel SDM Vol. 3A, section 6.10
	 */
	class Idt;
}

class Genode::Idt
{
	private:

		enum {
			SIZE_IDT    = 256,
			SYSCALL_VEC = 0x80,
		};

		/**
		 * 64-Bit Mode IDT gate, see Intel SDM Vol. 3A, section 6.14.1.
		 */
		struct gate
		{
			uint16_t offset_15_00;
			uint16_t segment_sel;
			uint16_t flags;
			uint16_t offset_31_16;
			uint32_t offset_63_32;
			uint32_t reserved;
		};

		/**
		 * IDT table
		 */
		__attribute__((aligned(8))) gate _table[SIZE_IDT];

	public:

		/**
		 * Setup IDT.
		 *
		 * \param virt_base  virtual base address of mode transition pages
		 */
		void setup(addr_t const virt_base);

		/**
		 * Load IDT into IDTR.
		 *
		 * \param virt_base  virtual base address of mode transition pages
		 */
		void load(addr_t const virt_base);
};

#endif /* _CORE__INCLUDE__SPEC__X86_64__IDT_H_ */
