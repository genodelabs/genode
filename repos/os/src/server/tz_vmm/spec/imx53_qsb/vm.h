/*
 * \brief  Virtual Machine implementation using device trees
 * \author Stefan Kalkowski
 * \date   2015-02-27
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SERVER__TZ_VMM__SPEC__IMX53_QSB__VM_H_
#define _SERVER__TZ_VMM__SPEC__IMX53_QSB__VM_H_

#include <vm_base.h>
#include <atag.h>

class Vm : public Vm_base
{
	private:

		enum {
			ATAG_OFFSET   = 0x100,
			INITRD_OFFSET = 0x1000000,
		};

		Genode::Rom_connection   _initrd_rom;
		Genode::Dataspace_client _initrd_cap;

		void _load_initrd()
		{
			using namespace Genode;
			addr_t addr = env()->rm_session()->attach(_initrd_cap);
			memcpy((void*)(_ram.local() + INITRD_OFFSET),
			       (void*)addr, _initrd_cap.size());
			env()->rm_session()->detach((void*)addr);
		}

		void _load_atag()
		{
			Atag tag((void*)(_ram.local() + ATAG_OFFSET));
			tag.setup_mem_tag(_ram.base(), _ram.size());
			tag.setup_cmdline_tag(_cmdline);
			tag.setup_initrd2_tag(_ram.base() + INITRD_OFFSET, _initrd_cap.size());
			if (_board_rev)
				tag.setup_rev_tag(_board_rev);
			tag.setup_end_tag();
		}

		/*
		 * Vm_base interface
		 */

		void _load_kernel_surroundings()
		{
			_load_initrd();
			_load_atag();
		}

		Genode::addr_t _board_info_offset() const { return ATAG_OFFSET; }

	public:

		Vm(const char *kernel, const char *cmdline,
		   Genode::addr_t ram_base, Genode::size_t ram_size,
		   Genode::addr_t kernel_offset, unsigned long mach_type,
		   unsigned long board_rev = 0)
		:
			Vm_base(kernel, cmdline, ram_base, ram_size,
			        kernel_offset, mach_type, board_rev),
			_initrd_rom("initrd.gz"),
			_initrd_cap(_initrd_rom.dataspace())
		{ }
};

#endif /* _SERVER__TZ_VMM__SPEC__IMX53_QSB__VM_H_ */
