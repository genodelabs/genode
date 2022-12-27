/*
 * \brief  Common functions for core VM-session component
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2018-08-26
 *
 * This file implements region-map functions for VM session, which
 * includes the 'Region_map_detach' interface. The latter is used if
 * an attached dataspace is destroyed.
 */

/*
 * Copyright (C) 2018-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Base includes */
#include <util/flex_iterator.h>

/* Core includes */
#include <cpu_thread_component.h>
#include <dataspace_component.h>
#include <vm_session_component.h>

using Genode::addr_t;
using Genode::Vm_session_component;

void Vm_session_component::attach(Dataspace_capability const cap,
                                  addr_t const guest_phys,
                                  Attach_attr attribute)
{
	if (!cap.valid())
		throw Invalid_dataspace();

	/* check dataspace validity */
	_ep.apply(cap, [&] (Dataspace_component *ptr) {
		if (!ptr)
			throw Invalid_dataspace();

		Dataspace_component &dsc = *ptr;

		/* unsupported - deny otherwise arbitrary physical memory can be mapped to a VM */
		if (dsc.managed())
			throw Invalid_dataspace();

		if (guest_phys & 0xffful || attribute.offset & 0xffful ||
		    attribute.size & 0xffful)
			throw Invalid_dataspace();

		if (!attribute.size) {
			attribute.size = dsc.size();

			if (attribute.offset < attribute.size)
				attribute.size -= attribute.offset;
		}

		if (attribute.size > dsc.size())
			attribute.size = dsc.size();

		if (attribute.offset >= dsc.size() ||
		    attribute.offset > dsc.size() - attribute.size)
			throw Invalid_dataspace();

		using Alloc_error = Range_allocator::Alloc_error;

		_map.alloc_addr(attribute.size, guest_phys).with_result(

			[&] (void *) {

				Rm_region::Attr const region_attr
				{
					.base  = guest_phys,
					.size  = attribute.size,
					.write = dsc.writeable() && attribute.writeable,
					.exec  = attribute.executable,
					.off   = (off_t)attribute.offset,
					.dma   = false,
				};

				/* store attachment info in meta data */
				try {
					_map.construct_metadata((void *)guest_phys,
					                        dsc, *this, region_attr);

				} catch (Allocator_avl_tpl<Rm_region>::Assign_metadata_failed) {
					error("failed to store attachment info");
					throw Invalid_dataspace();
				}

				Rm_region &region = *_map.metadata((void *)guest_phys);

				/* inform dataspace about attachment */
				dsc.attached_to(region);
			},

			[&] (Alloc_error error) {

				switch (error) {

				case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
				case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
				case Alloc_error::DENIED:
					{
						/*
						 * Handle attach after partial detach
						 */
						Rm_region *region_ptr = _map.metadata((void *)guest_phys);
						if (!region_ptr)
							throw Region_conflict();

						Rm_region &region = *region_ptr;

						if (!(cap == region.dataspace().cap()))
							throw Region_conflict();

						if (guest_phys < region.base() ||
						    guest_phys > region.base() + region.size() - 1)
							throw Region_conflict();
					}
				};
			}
		);

		/* kernel specific code to attach memory to guest */
		_attach_vm_memory(dsc, guest_phys, attribute);
	});
}


void Vm_session_component::detach(addr_t guest_phys, size_t size)
{
	if (!size || (guest_phys & 0xffful) || (size & 0xffful)) {
		warning("vm_session: skipping invalid memory detach addr=",
		        (void *)guest_phys, " size=", (void *)size);
		return;
	}

	addr_t const guest_phys_end = guest_phys + (size - 1);
	addr_t       addr           = guest_phys;
	do {
		Rm_region *region = _map.metadata((void *)addr);

		/* walk region holes page-by-page */
		size_t iteration_size = 0x1000;

		if (region) {
			iteration_size = region->size();

			/* inform dataspace */
			region->dataspace().detached_from(*region);

			/* cleanup metadata */
			_map.free(reinterpret_cast<void *>(region->base()));
		}

		if (addr >= guest_phys_end - (iteration_size - 1))
			break;

		addr += iteration_size;
	} while (true);

	/* kernel specific code to detach memory from guest */
	_detach_vm_memory(guest_phys, size);
}


void Vm_session_component::detach(Region_map::Local_addr addr)
{
	Rm_region *region = _map.metadata(addr);
	if (region)
		detach(region->base(), region->size());
	else
		Genode::error(__PRETTY_FUNCTION__, " unknown region");
}


void Vm_session_component::unmap_region(addr_t base, size_t size)
{
	Genode::error(__func__, " unimplemented ", base, " ", size);
}
