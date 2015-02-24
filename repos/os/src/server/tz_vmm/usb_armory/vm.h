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

class Vm : public Genode::Rom_connection,
		   public Genode::Dataspace_client,
		   public Vm_base
{
	private:

		enum { DTB_OFFSET = 0x2000000 };

		Genode::addr_t _load_board_info()
		{
			using namespace Genode;

			addr_t addr = env()->rm_session()->attach(*this);
			memcpy((void*)(_ram.local() + DTB_OFFSET), (void*)addr, size());
			env()->rm_session()->detach((void*)addr);
			return DTB_OFFSET;
		}

	public:

		Vm(const char *kernel, const char *initrd, const char *cmdline,
		   Genode::addr_t ram_base, Genode::size_t ram_size,
		   Genode::addr_t kernel_offset, unsigned long mach_type,
		   unsigned long board_rev = 0)
		: Genode::Rom_connection("dtb"),
		  Genode::Dataspace_client(dataspace()),
		  Vm_base(kernel, initrd, cmdline, ram_base, ram_size,
		          kernel_offset, mach_type, board_rev) {}
};
