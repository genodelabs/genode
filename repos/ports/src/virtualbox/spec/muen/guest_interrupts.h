/*
 * \brief  Muen subject pending interrupt handling
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2016-04-27
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__MUEN__INTERRUPTS_H_
#define _VIRTUALBOX__SPEC__MUEN__INTERRUPTS_H_

/* Genode includes */
#include <base/stdint.h>
#include <os/attached_io_mem_dataspace.h>

namespace Genode
{
	/**
	 * Vm execution handler.
	 */
	class Guest_interrupts;
}


class Genode::Guest_interrupts
{
	private:

		addr_t _base;

	public:

		Guest_interrupts (addr_t base) : _base(base) { }

		/**
		 * Returns true if the bit corresponding to the specified IRQ is set.
		 */
		bool is_pending_interrupt(uint8_t irq)
		{
			bool result;
			asm volatile ("bt %1, %2;"
					"sbb %0, %0;"
					: "=r" (result)
					: "Ir" ((uint32_t)irq), "m" (*(char *)_base)
					: "memory");
			return result;
		}

		/**
		 * Set bit corresponding to given IRQ in pending interrupts region.
		 */
		void set_pending_interrupt(uint8_t irq)
		{
			asm volatile ("lock bts %1, %0"
					: "+m" (*(char *)_base)
					: "Ir" ((uint32_t)irq)
					: "memory");
		}

		/**
		 * Clear bit corresponding to given IRQ in pending interrupts region.
		 */
		void clear_pending_interrupt(uint8_t irq)
		{
			asm volatile ("lock btr %1, %0"
					: "+m" (*(char *)_base)
					: "Ir" ((uint32_t)irq)
					: "memory");
		}
};

#endif /* _VIRTUALBOX__SPEC__MUEN__INTERRUPTS_H_ */
