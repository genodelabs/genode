/*
 * \brief   Utilities for manipulating seL4 CNodes
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__UNTYPED_ADDRESS_H_
#define _CORE__INCLUDE__UNTYPED_ADDRESS_H_

/* seL4 includes */
#include <sel4/bootinfo.h>

namespace Genode { struct Untyped_address; }


/**
 * Obtain seL4 boot info structure
 */
static inline seL4_BootInfo const &sel4_boot_info()
{
	extern Genode::addr_t __initial_bx;
	return *(seL4_BootInfo const *)__initial_bx;
}


/**
 * Untuped memory address
 *
 * When referring to physical memory in seL4 system-call arguments, a memory
 * address is specified as a tuple of an untyped memory range selector and the
 * offset relative to the base address of the untyped memory range.
 */
class Genode::Untyped_address
{
	private:

		seL4_Untyped _sel    = 0;
		addr_t       _offset = 0;
		addr_t       _phys   = 0;

		void _init(seL4_BootInfo const &bi, addr_t phys_addr, size_t size,
		           unsigned const start_idx, unsigned const num_idx)
		{
			for (unsigned i = start_idx; i < start_idx + num_idx; i++) {

				/* index into 'untypedPaddrList' and 'untypedSizeBitsList' */
				unsigned const k = i - bi.untyped.start;

				addr_t const untyped_base = bi.untypedPaddrList[k];
				size_t const untyped_size = 1UL << bi.untypedSizeBitsList[k];

				if (phys_addr >= untyped_base
				 && phys_addr + size - 1 <= untyped_base + untyped_size - 1) {

					_sel    = i;
					_offset = phys_addr - untyped_base;
					return;
				}
			}
		}

	public:

		class Lookup_failed { };

		/**
		 * Construct untyped address for given physical address range
		 *
		 * \throw Lookup_failed
		 */
		Untyped_address(addr_t phys_addr, size_t size)
		{
			_phys = phys_addr;

			seL4_BootInfo const &bi = sel4_boot_info();
			_init(bi, phys_addr, size, bi.untyped.start,
			      bi.untyped.end - bi.untyped.start);

			/* XXX handle untyped device addresses */

			if (_sel == 0) {
				PERR("could not find untyped address for 0x%lx", phys_addr);
				throw Lookup_failed();
			}
		}

		unsigned sel()    const { return _sel; }
		addr_t   offset() const { return _offset; }
		addr_t   phys()   const { return _phys; }
};


#endif /* _CORE__INCLUDE__UNTYPED_ADDRESS_H_ */
