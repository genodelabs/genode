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

#ifndef _SERVER__TZ_VMM__SPEC__USB_ARMORY__VM_H_
#define _SERVER__TZ_VMM__SPEC__USB_ARMORY__VM_H_

/* local includes */
#include <vm_base.h>

class Vm : public Genode::Rom_connection,
           public Genode::Dataspace_client,
           public Vm_base
{
	private:

		enum { DTB_OFFSET = 0x1000000 };

		void _load_dtb()
		{
			using namespace Genode;
			addr_t addr = env()->rm_session()->attach(*this);
			memcpy((void*)(_ram.local() + DTB_OFFSET), (void*)addr, size());
			env()->rm_session()->detach((void*)addr);
		}

		/*
		 * Vm_base interface
		 */

		void _load_kernel_surroundings() { _load_dtb(); }

		Genode::addr_t _board_info_offset() const { return DTB_OFFSET; }

	public:

		Vm(const char *kernel, const char *cmdline,
		   Genode::addr_t ram_base, Genode::size_t ram_size,
		   Genode::addr_t kernel_offset, unsigned long mach_type,
		   unsigned long board_rev = 0)
		: Genode::Rom_connection("dtb"),
		  Genode::Dataspace_client(dataspace()),
		  Vm_base(kernel, cmdline, ram_base, ram_size,
		          kernel_offset, mach_type, board_rev)
		{ }
};

#endif /* _SERVER__TZ_VMM__SPEC__USB_ARMORY__VM_H_ */
