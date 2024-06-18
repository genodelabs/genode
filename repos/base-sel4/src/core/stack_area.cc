/*
 * \brief  Support code for the thread API
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <region_map/region_map.h>
#include <base/ram_allocator.h>
#include <base/synced_allocator.h>
#include <base/thread.h>

/* local includes */
#include <platform.h>
#include <map_local.h>
#include <dataspace_component.h>
#include <untyped_memory.h>

/* base-internal includes */
#include <base/internal/stack_area.h>
#include <base/internal/globals.h>

using namespace Genode;


/**
 * Region-manager session for allocating stacks
 *
 * This class corresponds to the managed dataspace that is normally used for
 * organizing stacks with the stack area. In contrast to the ordinary
 * implementation, core's version does not split between allocation of memory
 * and virtual memory management. Due to the missing availability of "real"
 * dataspaces and capabilities refering to it without having an entrypoint in
 * place, the allocation of a dataspace has no effect, but the attachment of
 * the thereby "empty" dataspace is doing both: allocation and attachment.
 */
class Stack_area_region_map : public Region_map
{
	private:

		using Ds_slab = Synced_allocator<Tslab<Core::Dataspace_component,
		                                       get_page_size()> >;

		Ds_slab _ds_slab { Core::platform().core_mem_alloc() };

	public:

		/**
		 * Allocate and attach on-the-fly backing store to the stack area
		 */
		Attach_result attach(Dataspace_capability, Attr const &attr) override
		{
			using namespace Core;

			size_t const size      = round_page(attr.size);
			size_t const num_pages = size >> get_page_size_log2();

			/* allocate physical memory */
			Range_allocator &phys_alloc = Core::platform_specific().ram_alloc();
			addr_t const phys = Untyped_memory::alloc_pages(phys_alloc, num_pages);
			Untyped_memory::convert_to_page_frames(phys, num_pages);

			Dataspace_component &ds = *new (&_ds_slab)
				Dataspace_component(size, 0, phys, CACHED, true, 0);

			addr_t const core_local_addr = stack_area_virtual_base() + attr.at;

			if (!map_local(ds.phys_addr(), core_local_addr,
			               ds.size() >> get_page_size_log2())) {
				error(__func__, ": could not map phys ", Hex(ds.phys_addr()), " "
				      "at local ", Hex(core_local_addr));
				return Attach_error::INVALID_DATASPACE;
			}

			ds.assign_core_local_addr((void*)core_local_addr);

			return Range { .start = attr.at, .num_bytes = size };
		}

		void detach(addr_t at) override
		{
			using namespace Core;

			if (at >= stack_area_virtual_size())
				return;

			addr_t const detach = stack_area_virtual_base() + at;
			addr_t const stack  = stack_virtual_size();
			addr_t const pages  = ((detach & ~(stack - 1)) + stack - detach)
			                      >> get_page_size_log2();

			unmap_local(detach, pages);

			/* XXX missing XXX */
			warning(__PRETTY_FUNCTION__, ": not implemented");
		}

		void fault_handler(Signal_context_capability) override { }

		Fault fault() override { return { }; }

		Dataspace_capability dataspace() override { return { }; }
};


struct Stack_area_ram_allocator : Ram_allocator
{
	Alloc_result try_alloc(size_t, Cache) override {
		return reinterpret_cap_cast<Ram_dataspace>(Native_capability()); }

	void free(Ram_dataspace_capability) override {
		warning(__func__, " not implemented"); }

	size_t dataspace_size(Ram_dataspace_capability) const override { return 0; }
};


namespace Genode {

	Region_map    *env_stack_area_region_map;
	Ram_allocator *env_stack_area_ram_allocator;

	void init_stack_area()
	{
		static Stack_area_region_map rm_inst;
		env_stack_area_region_map = &rm_inst;

		static Stack_area_ram_allocator ram_inst;
		env_stack_area_ram_allocator = &ram_inst;
	}
}
