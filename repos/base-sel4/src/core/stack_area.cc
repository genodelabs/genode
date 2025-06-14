/*
 * \brief  Support code for the thread API
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Alexander Boettcher
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

/* local includes */
#include <platform.h>
#include <map_local.h>
#include <dataspace_component.h>
#include <untyped_memory.h>

/* base-internal includes */
#include <base/internal/stack_area.h>
#include <base/internal/globals.h>

namespace Genode {

	Region_map    *env_stack_area_region_map;
	Ram_allocator *env_stack_area_ram_allocator;

	void init_stack_area();
}


using namespace Core;


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

		enum { CORE_MAX_THREADS = stack_area_virtual_size() /
		                          stack_virtual_size() };
		struct {
			Allocator::Result phys { };
			addr_t            core_local_addr { };
		} _core_stacks_phys_to_virt[CORE_MAX_THREADS];

		Attach_result with_phys_result(addr_t const at, auto const &fn)
		{
			if (at >= stack_area_virtual_size())
				return Attach_result(Attach_error::INVALID_DATASPACE);

			auto stack_id = at / stack_virtual_size();
			if (stack_id >= CORE_MAX_THREADS)
				return Attach_result(Attach_error::INVALID_DATASPACE);

			return fn(_core_stacks_phys_to_virt[stack_id].phys);
		}

	public:

		/**
		 * Allocate and attach on-the-fly backing store to the stack area
		 */
		Attach_result attach(Dataspace_capability, Attr const &attr) override
		{
			return with_phys_result(attr.at, [&](auto &phys_result) {

				Range_allocator &phys_alloc = platform_specific().ram_alloc();

				size_t const size      = round_page(attr.size);
				size_t const num_pages = size >> get_page_size_log2();

				/* allocate physical memory */
				phys_result = Untyped_memory::alloc_pages(phys_alloc, num_pages);

				return phys_result.template convert<Attach_result>([&](auto &result) {
					addr_t const phys = addr_t(result.ptr);

					if (!Untyped_memory::convert_to_page_frames(phys, num_pages))
						return Attach_result(Attach_error::INVALID_DATASPACE);

					addr_t const core_local_addr = stack_area_virtual_base() + attr.at;

					if (!map_local(phys, core_local_addr, num_pages)) {
						error(__func__, ": could not map phys ", Hex(phys), " "
						      "at local ", Hex(core_local_addr));
						return Attach_result(Attach_error::INVALID_DATASPACE);
					}

					return Attach_result(Range { .start = attr.at, .num_bytes = size });
				}, [&](auto) {
					return Attach_result(Attach_error::INVALID_DATASPACE); });
			});
		}

		void detach(addr_t const at) override
		{
			auto result = with_phys_result(at, [&](auto &phys_result) {

				addr_t const detach = stack_area_virtual_base() + at;
				addr_t const stack  = stack_virtual_size();
				/* calculate stack and ipc buffer size */
				addr_t const size_i = (detach & ~(stack - 1)) + stack - detach;
				/* remove IPC buffer size */
				addr_t const size   = size_i >= 4096 ? size_i - 4096 : size_i;
				addr_t const pages  = size >> get_page_size_log2();

				unmap_local(detach, pages);

				phys_result.with_result([&](auto &result) {
					Untyped_memory::convert_to_untyped_frames(addr_t(result.ptr), size);
				}, [](auto) { });

				/* deallocate phys */
				phys_result = { };

				return Attach_result(Range { .start = at, .num_bytes = 0 });
			});

			if (result.failed())
				error(__func__, " failed");
		}

		void fault_handler(Signal_context_capability) override { }

		Fault fault() override { return { }; }

		Dataspace_capability dataspace() override { return { }; }
};


struct Stack_area_ram_allocator : Ram_allocator
{
	Result try_alloc(size_t, Cache) override { return { *this, { } }; }

	void _free(Ram::Allocation &) override { }
};


void Genode::init_stack_area()
{
	static Stack_area_region_map rm;
	env_stack_area_region_map = &rm;

	static Stack_area_ram_allocator ram;
	env_stack_area_ram_allocator = &ram;
}
