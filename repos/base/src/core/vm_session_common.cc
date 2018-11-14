/*
 * \brief  Core-specific instance of the VM session interface
 * \author Alexander Boettcher
 * \date   2018-08-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
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

		switch (_map.alloc_addr(attribute.size, guest_phys).value) {
		case Range_allocator::Alloc_return::OUT_OF_METADATA:
			throw Out_of_ram();
		case Range_allocator::Alloc_return::RANGE_CONFLICT:
		{
			Rm_region *region_ptr = _map.metadata((void *)guest_phys);
			if (!region_ptr)
				throw Region_conflict();

			Rm_region &region = *region_ptr;

			if (!(cap == region.dataspace().cap()))
				throw Region_conflict();
			if (guest_phys < region.base() ||
			    guest_phys > region.base() + region.size() - 1)
				throw Region_conflict();

			/* re-attach all */
			break;
		}
		case Range_allocator::Alloc_return::OK:
		{
			/* store attachment info in meta data */
			try {
				_map.construct_metadata((void *)guest_phys,
				                        guest_phys, attribute.size,
				                        dsc.writable() && attribute.writeable,
				                        dsc, attribute.offset, *this,
				                        attribute.executable);
			} catch (Allocator_avl_tpl<Rm_region>::Assign_metadata_failed) {
				error("failed to store attachment info");
				throw Invalid_dataspace();
			}

			Rm_region &region = *_map.metadata((void *)guest_phys);

			/* inform dataspace about attachment */
			dsc.attached_to(region);

			break;
		}
		};

		/* kernel specific code to attach memory to guest */
		_attach_vm_memory(dsc, guest_phys, attribute);
	});
}

void Vm_session_component::detach(addr_t guest_phys, size_t size)
{
	if (guest_phys & 0xffful) {
		size += 0x1000 - (guest_phys & 0xffful);
		guest_phys &= ~0xffful;
	}

	if (size & 0xffful)
		size = align_addr(size, 12);

	if (!size)
		return;

	{
		Rm_region *region = _map.metadata(reinterpret_cast<void *>(guest_phys));
		if (region && guest_phys == region->base() && region->size() <= size) {
			/* inform dataspace */
			region->dataspace().detached_from(*region);
			/* cleanup metadata */
			_map.free(reinterpret_cast<void *>(region->base()));
		}
	}

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
