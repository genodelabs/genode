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

#include <vm_base.h>
#include <atag.h>

class Vm : public Vm_base
{
	private:

		enum { ATAG_OFFSET = 0x100 };

		Genode::addr_t _load_board_info()
		{
			Atag tag((void*)(_ram.local() + ATAG_OFFSET));
			tag.setup_mem_tag(_ram.base(), _ram.size());
			tag.setup_cmdline_tag(_cmdline);
			tag.setup_initrd2_tag(_ram.base() + INITRD_OFFSET, _initrd_cap.size());
			if (_board_rev)
				tag.setup_rev_tag(_board_rev);
			tag.setup_end_tag();
			return ATAG_OFFSET;
		}

	public:

		Vm(const char *kernel, const char *initrd, const char *cmdline,
		   Genode::addr_t ram_base, Genode::size_t ram_size,
		   Genode::addr_t kernel_offset, unsigned long mach_type,
		   unsigned long board_rev = 0)
		: Vm_base(kernel, initrd, cmdline, ram_base, ram_size,
		          kernel_offset, mach_type, board_rev) {}
};
