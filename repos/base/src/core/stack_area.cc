/*
 * \brief  Support code for the thread API
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <region_map/region_map.h>
#include <pd_session/pd_session.h>
#include <base/thread.h>
#include <util/bit_allocator.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

/* local includes */
#include <platform.h>
#include <map_local.h>


namespace Genode {

	Region_map    *env_stack_area_region_map;
	Ram_allocator *env_stack_area_ram_allocator;

	void init_stack_area();
}


using namespace Core;


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

		static int constexpr MAX_STACKS = stack_area_virtual_size()
		                                / stack_virtual_size();

		Mutex                     _mutex { };
		Bit_allocator<MAX_STACKS> _alloc { };

		Allocation<Memory::Constrained_allocator>::Attempt _phys_stacks[MAX_STACKS];

		Attach_result with_new_stack(auto const &fn, auto const &fn_error)
		{
			return _alloc.alloc().template convert<Attach_result>(
				[&] (addr_t const index) -> Attach_result {
					auto &phys_stack = _phys_stacks[index];

					auto result  = fn(phys_stack);

					if (!result.ok()) {
						/* release of physical memory allocation and id */
						phys_stack = { };
						_alloc.free(index);
					}

					return result;
			}, [&] (auto e) { return fn_error(e); });
		}

	public:

		/**
		 * Allocate and attach on-the-fly backing store to stack area
		 */
		Attach_result attach(Dataspace_capability, Attr const &attr) override;

		void detach(addr_t const at) override;

		void fault_handler(Signal_context_capability) override { }

		Fault fault() override { return { }; }

		Dataspace_capability dataspace() override { return { }; }
};


void Stack_area_region_map::detach(addr_t const at)
{
	Mutex::Guard guard(_mutex);

	addr_t const detach = stack_area_virtual_base() + at;
	addr_t const stack  = stack_virtual_size();
	addr_t const pages  = ((detach & ~(stack - 1)) + stack - detach)
	                      >> get_page_size_log2();
	addr_t const id     = at / stack;

	if (at >= stack_area_virtual_size() || id >= MAX_STACKS) {
		error("unexpected detach of core stack");
		return;
	}

	unmap_local(detach, pages);

	/* release of physical memory allocation and id */
	_phys_stacks[id] = { };
	_alloc.free(id);
}


Stack_area_region_map::Attach_result Stack_area_region_map::attach(Dataspace_capability, Attr const &attr)
{
	Mutex::Guard guard(_mutex);

	size_t const size = round_page(attr.size);

	return with_new_stack([&](auto &phys_stack) {

		Range_allocator &alloc = platform_specific().ram_alloc();

		/* allocate physical memory */
		phys_stack = alloc.alloc_aligned(size, get_page_size_log2());

		return phys_stack.template convert<Attach_result>([&] (auto &phys) -> Attach_result {

			addr_t const core_local_addr = stack_area_virtual_base()
			                             + attr.at;

			if (!map_local(addr_t(phys.ptr), core_local_addr,
			               size >> get_page_size_log2())) {
				error("could not map phys ", phys.ptr,
				      " at local ", Hex(core_local_addr));

				return Attach_error::INVALID_DATASPACE;
			}

			return Range { .start = attr.at, .num_bytes = size };

		}, [&] (auto) {
			error("could not allocate backing store for new stack");
			return Attach_error::REGION_CONFLICT;
		});

	}, [&] (auto) {

		error("could not allocate backing store for new stack");
		return Attach_error::REGION_CONFLICT;
	});
}


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
