/*
 * \brief  x86 emulation binding and support
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _IFX86EMU_H_
#define _IFX86EMU_H_

#include <base/stdint.h>


namespace Genode {
	struct Env;
	struct Allocator;
}

namespace X86emu {

	using Genode::addr_t;
	using Genode::uint16_t;

	enum {
		PAGESIZE = 0x01000,      /* x86 page size */
		CODESIZE = 2 * PAGESIZE, /* size of fake code segment */
	};

	/**
	 * Emulation memory/code area structure
	 */
	struct X86emu_mem
	{
			addr_t _bios_addr;
			addr_t _data_addr;

			X86emu_mem() : _bios_addr(0), _data_addr(0) { }

			inline addr_t _addr(addr_t* obj_addr, void* addr = 0)
			{
				if (addr != 0)
					*obj_addr = reinterpret_cast<addr_t>(addr);
				return *obj_addr;
			}

			inline addr_t bios_addr(void *addr = 0) { return _addr(&_bios_addr, addr); }
			inline addr_t data_addr(void *addr = 0) { return _addr(&_data_addr, addr); }
	};

	/**
	 * Memory/code area instance
	 */
	extern struct X86emu_mem x86_mem;

	/**
	 * Initialization
	 */
	void init(Genode::Env &, Genode::Allocator &);

	/**
	 * Execute real mode command
	 */
	uint16_t x86emu_cmd(uint16_t eax, uint16_t ebx = 0, uint16_t ecx = 0,
	                    uint16_t edi = 0, uint16_t *out_ebx = 0);

	/**
	 * Map requested address to local virtual address
	 *
	 * Note, virtual addresses cannot be cached as mappings may change on
	 * 'x86emu_cmd' and subsequent invocations of this function.
	 */
	template <typename TYPE, typename ADDR_TYPE>
	TYPE * virt_addr(ADDR_TYPE addr);

	/**
	 * Log I/O resources for debugging
	 */
	void print_regions();
}

#endif /* _IFX86EMU_H_ */
