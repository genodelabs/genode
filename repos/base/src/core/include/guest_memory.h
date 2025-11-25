/*
 * \brief  Guest memory abstraction
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2024-11-25
 */

/*
 * Copyright (C) 2015-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__GUEST_MEMORY_H_
#define _CORE__GUEST_MEMORY_H_

/* base includes */
#include <base/allocator_avl.h>
#include <base/heap.h>
#include <dataspace/capability.h>
#include <vm_session/vm_session.h>

/* core includes */
#include <dataspace_component.h>
#include <region_map_component.h>

namespace Core { class Guest_memory; }

class Core::Guest_memory
{
	public:

		enum class Attach_result {
			OK,
			INVALID_DS,
			OUT_OF_RAM,
			OUT_OF_CAPS,
			REGION_CONFLICT,
		};

	private:

		using Avl_region = Allocator_avl_tpl<Rm_region>;

		using Attach_attr = Genode::Vm_session::Attach_attr;

		Rpc_entrypoint &_ep;

		Region_map_detach &_detach;

		Sliced_heap _sliced_heap;
		Avl_region  _map { &_sliced_heap };

		uint8_t _remaining_print_count { 10 };

		void error(auto &&... args)
		{
			if (_remaining_print_count) {
				Genode::error(args...);
				_remaining_print_count--;
			}
		}

		void _with_region(addr_t const addr, auto const &fn)
		{
			Rm_region *region = _map.metadata((void *)addr);
			if (region)
				fn(*region);
			else
				error("unknown region");
		}

		Attach_result _attach(Dataspace_component &dsc,
		                      addr_t const         guest_phys,
		                      Attach_attr         &attr)
		{
			/*
			 * unsupported - deny otherwise arbitrary physical
			 * memory can be mapped to a VM
			 */
			if (dsc.managed())
				return Attach_result::INVALID_DS;

			auto page_aligned = [] (addr_t addr) {
				return aligned(addr, get_page_size_log2()); };

			if (!page_aligned(guest_phys) ||
			    !page_aligned(attr.offset) ||
			    !page_aligned(attr.size))
				return Attach_result::INVALID_DS;

			if (!attr.size) {
				attr.size = dsc.size();

				if (attr.offset < attr.size)
					attr.size -= attr.offset;
			}

			if (attr.size > dsc.size())
				attr.size = dsc.size();

			if (attr.offset >= dsc.size() ||
			    attr.offset > dsc.size() - attr.size)
				return Attach_result::INVALID_DS;

			return _map.alloc_addr(attr.size, guest_phys).convert<Attach_result>(

				[&] (Range_allocator::Allocation &allocation) {

					Rm_region::Attr const region_attr
					{
						.base  = guest_phys,
						.size  = attr.size,
						.write = dsc.writeable() && attr.writeable,
						.exec  = attr.executable,
						.off   = attr.offset,
						.dma   = false,
					};

					/* store attachment info in meta data */
					if (!_map.construct_metadata((void *)guest_phys,
					                             dsc, _detach, region_attr)) {
						error("failed to store attachment info");
						return Attach_result::INVALID_DS;
					}

					Rm_region &region = *_map.metadata((void *)guest_phys);

					/* inform dataspace about attachment */
					dsc.attached_to(region);

					allocation.deallocate = false;
					return Attach_result::OK;
				},

				[&] (Alloc_error error) {

					switch (error) {

					case Alloc_error::OUT_OF_RAM:
						return Attach_result::OUT_OF_RAM;
					case Alloc_error::OUT_OF_CAPS:
						return Attach_result::OUT_OF_CAPS;
					case Alloc_error::DENIED:
						break;
					};

					/*
					 * Handle attach after partial detach
					 */
					Rm_region *region_ptr = _map.metadata((void *)guest_phys);
					if (!region_ptr)
						return Attach_result::REGION_CONFLICT;

					Rm_region &region = *region_ptr;

					bool conflict = false;
					region.with_dataspace([&] (Dataspace_component &ds) {
						conflict = dsc.cap() != ds.cap(); });
					if (conflict)
						return Attach_result::REGION_CONFLICT;

					if (guest_phys < region.base() ||
					    guest_phys > region.base() + region.size() - 1)
						return Attach_result::REGION_CONFLICT;

					return Attach_result::OK;
				}
			);
		}

	public:

		Attach_result attach(Dataspace_capability cap,
		                     addr_t const         guest_phys,
		                     Attach_attr          attr,
		                     auto const          &map_fn)
		{
			if (!cap.valid())
				return Attach_result::INVALID_DS;

			Attach_result ret = Attach_result::INVALID_DS;

			/* check dataspace validity */
			_ep.apply(cap, [&] (Dataspace_component *dsc) {
				if (!dsc)
					return;

				ret = _attach(*dsc, guest_phys, attr);
				if (ret != Attach_result::OK)
					return;

				ret = map_fn(guest_phys, dsc->phys_addr() + attr.offset,
				             attr.size, attr.executable,
				             attr.writeable && dsc->writeable(),
				             dsc->cacheability());
			});

			return ret;
		}


		void detach(addr_t     guest_phys,
		            size_t     size,
		            auto const &unmap_fn)
		{
			auto page_aligned = [] (addr_t addr) {
				return aligned(addr, get_page_size_log2()); };

			if (!size ||
			    !page_aligned(guest_phys) ||
			    !page_aligned(size)) {
				error("vm_session: skipping invalid memory detach addr=",
				      (void *)guest_phys, " size=", (void *)size);
				return;
			}

			addr_t const guest_phys_end = guest_phys + (size - 1);
			addr_t       addr           = guest_phys;
			do {
				Rm_region *region = _map.metadata((void *)addr);

				/* walk region holes page-by-page */
				size_t iteration_size = get_page_size();

				if (region) {
					iteration_size = region->size();
					detach_at(region->base(), unmap_fn);
				}

				if (addr >= guest_phys_end - (iteration_size - 1))
					break;

				addr += iteration_size;
			} while (true);
		}


		Guest_memory(Rpc_entrypoint &ep, Region_map_detach &detach,
		             Accounted_ram_allocator &ram, Local_rm &local_rm)
		:
			_ep(ep), _detach(detach), _sliced_heap(ram, local_rm)
		{
			/**
			 * Configure managed VM area,
			 * unfortunately we cannot add the whole range in once,
			 * because the size type is limited, therefore add the
			 * last byte separatedly
			 */
			if (_map.add_range(0UL, ~0UL).failed() ||
			    _map.add_range(~0UL, 1).failed())
				error("unable to initialize guest-memory allocator");
		}

		~Guest_memory()
		{
			/* detach all regions */
			while (true) {
				addr_t out_addr = 0;

				if (!_map.any_block_addr(&out_addr))
					break;

				_detach.detach_at(out_addr);
			}
		}


		void detach_at(addr_t addr,
		               auto const &unmap_fn)
		{
			_with_region(addr, [&] (Rm_region &region) {

				if (!region.reserved())
					reserve_and_flush(addr, unmap_fn);

				/* free the reserved region */
				_map.free(reinterpret_cast<void *>(region.base()));
			});
		}


		void reserve_and_flush(addr_t addr,
		                       auto const &unmap_fn)
		{
			_with_region(addr, [&] (Rm_region &region) {

				/* inform dataspace */
				region.with_dataspace([&] (Dataspace_component &dataspace) {
					dataspace.detached_from(region);
				});

				region.mark_as_reserved();

				unmap_fn(region.base(), region.size());
			});
		}
};

#endif /* _CORE__GUEST_MEMORY_H_ */
