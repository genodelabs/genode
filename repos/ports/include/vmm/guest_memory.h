/*
 * \brief  Utilities for implementing VMMs on Genode/NOVA
 * \author Norman Feske
 * \date   2013-08-20
 *
 * The VMM and the guest share the same PD. However, the guest's view on the PD
 * is restricted to the guest-physical-to-VMM-local mappings installed by the
 * VMM for the VCPU's EC.
 *
 * The guest memory is shadowed at the lower portion of the VMM's address
 * space. If the guest (the VCPU EC) tries to access a page that has no mapping
 * in the VMM's PD, NOVA does not generate a page-fault (which would be
 * delivered to the pager of the VMM, i.e., core) but it produces a NPT
 * virtualization event handled locally by the VMM. The NPT event handler is
 * the '_svm_npt' function.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM__GUEST_MEMORY_H_
#define _INCLUDE__VMM__GUEST_MEMORY_H_

/* Genode includes */
#include <os/attached_ram_dataspace.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

/* VMM utilities includes */
#include <vmm/types.h>

namespace Vmm {
	using namespace Genode;

	class Virtual_reservation;
	class Guest_memory;
}


/**
 * The 'Virtual_reservation' is a managed dataspace that occupies the lower
 * part of the address space, which contains the shadow of the VCPU's physical
 * memory.
 */
struct Vmm::Virtual_reservation : private Rm_connection, Region_map_client
{
	Virtual_reservation(addr_t vm_size)
	:
		Region_map_client(Rm_connection::create(vm_size))
	{
		try {
			/*
			 * Attach reservation to the beginning of the local address
			 * space. We leave out the very first page because core denies
			 * the attachment of anything at the zero page.
			 */
			env()->rm_session()->attach_at(Region_map_client::dataspace(),
			                               PAGE_SIZE, 0, PAGE_SIZE);

		} catch (Rm_session::Region_conflict) {
			error("region conflict while attaching guest-physical memory");
		}
	}

	~Virtual_reservation()
	{
		env()->rm_session()->detach((void *)PAGE_SIZE);
	}
};


/**
 * Representation of guest memory
 *
 */
struct Vmm::Guest_memory : Attached_ram_dataspace
{
	/**
	 * Constructor
	 *
	 * \param backing_store_size  number of bytes of physical RAM to be
	 *                            used as guest-physical and device memory,
	 *                            allocated from core's RAM service
	 */
	Guest_memory(size_t backing_store_size)
	:
		Attached_ram_dataspace(env()->ram_session(), backing_store_size)
	{ }
};


#endif /* _INCLUDE__VMM__GUEST_MEMORY_H_ */
