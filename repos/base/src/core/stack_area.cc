/*
 * \brief  Support code for the thread API
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <region_map/region_map.h>
#include <ram_session/ram_session.h>
#include <base/log.h>
#include <base/synced_allocator.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

/* local includes */
#include <platform.h>
#include <map_local.h>
#include <dataspace_component.h>


namespace Genode {

	Region_map  *env_stack_area_region_map;
	Ram_session *env_stack_area_ram_session;

	void init_stack_area();
}


using namespace Genode;


/**
 * Region-manager session for allocating stacks
 *
 * This class corresponds to the managed dataspace that is normally
 * used for organizing stacks within the stack area.
 * In contrast to the ordinary implementation, core's version does
 * not split between allocation of memory and virtual memory management.
 * Due to the missing availability of "real" dataspaces and capabilities
 * referring to it without having an entrypoint in place, the allocation
 * of a dataspace has no effect, but the attachment of the thereby "empty"
 * dataspace is doing both: allocation and attachment.
 */
class Stack_area_region_map : public Region_map
{
	private:

		using Ds_slab = Synced_allocator<Tslab<Dataspace_component,
		                                       get_page_size()> >;

		Ds_slab _ds_slab { platform()->core_mem_alloc() };

	public:

		/**
		 * Allocate and attach on-the-fly backing store to stack area
		 */
		Local_addr attach(Dataspace_capability ds_cap, /* ignored capability */
		                  size_t size, off_t offset,
		                  bool use_local_addr, Local_addr local_addr,
		                  bool executable) override
		{
			/* allocate physical memory */
			size = round_page(size);
			void *phys_base;
			Range_allocator *ra = platform_specific()->ram_alloc();
			if (ra->alloc_aligned(size, &phys_base,
				                  get_page_size_log2()).error()) {
				error("could not allocate backing store for new stack");
				return (addr_t)0;
			}

			Dataspace_component *ds = new (&_ds_slab)
				Dataspace_component(size, 0, (addr_t)phys_base, CACHED, true, 0);
			if (!ds) {
				error("dataspace for core stack does not exist");
				return (addr_t)0;
			}

			addr_t core_local_addr = stack_area_virtual_base() + (addr_t)local_addr;

			if (!map_local(ds->phys_addr(), core_local_addr,
			               ds->size() >> get_page_size_log2())) {
				error("could not map phys ", Hex(ds->phys_addr()),
				      " at local ", Hex(core_local_addr));
				return (addr_t)0;
			}

			ds->assign_core_local_addr((void*)core_local_addr);

			return local_addr;
		}

		void detach(Local_addr local_addr) override
		{
			using Genode::addr_t;

			if ((addr_t)local_addr >= stack_area_virtual_size())
				return;

			addr_t const detach = stack_area_virtual_base() + (addr_t)local_addr;
			addr_t const stack  = stack_virtual_size();
			addr_t const pages  = ((detach & ~(stack - 1)) + stack - detach)
			                      >> get_page_size_log2();

			unmap_local(detach, pages);
		}

		void fault_handler(Signal_context_capability) override { }

		State state() override { return State(); }

		Dataspace_capability dataspace() override { return Dataspace_capability(); }
};


class Stack_area_ram_session : public Ram_session
{
	public:

		Ram_dataspace_capability alloc(size_t, Cache_attribute) override {
			return reinterpret_cap_cast<Ram_dataspace>(Native_capability()); }

		void   free           (Ram_dataspace_capability)       override { }
		int    ref_account    (Ram_session_capability)         override { return 0; }
		int    transfer_quota (Ram_session_capability, size_t) override { return 0; }
		size_t quota          ()                               override { return 0; }
		size_t used           ()                               override { return 0; }
};


void Genode::init_stack_area()
{
	static Stack_area_region_map rm;
	env_stack_area_region_map = &rm;

	static Stack_area_ram_session ram;
	env_stack_area_ram_session = &ram;
}
