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
#include <base/allocator.h>
#include <base/allocator_avl.h>
#include <vm_session/vm_session.h>
#include <dataspace/capability.h>

/* core includes */
#include <dataspace_component.h>
#include <region_map_component.h>

namespace Core { class Guest_memory; }

using namespace Core;


class Core::Guest_memory
{
	private:

		using Avl_region = Allocator_avl_tpl<Rm_region>;

		using Attach_attr = Genode::Vm_session::Attach_attr;

		Sliced_heap          _sliced_heap;
		Avl_region           _map { &_sliced_heap };

		uint8_t _remaining_print_count { 10 };

		void _with_region(addr_t const addr, auto const &fn)
		{
			Rm_region *region = _map.metadata((void *)addr);
			if (region)
				fn(*region);
			else
				if (_remaining_print_count) {
					error(__PRETTY_FUNCTION__, " unknown region");
					_remaining_print_count--;
				}
		}

	public:

		enum class Attach_result {
			OK,
			INVALID_DS,
			OUT_OF_RAM,
			OUT_OF_CAPS,
			REGION_CONFLICT,
		};


		Attach_result attach(Region_map_detach   &rm_detach,
		                     Dataspace_component &dsc,
		                     addr_t const         guest_phys,
		                     Attach_attr          attr,
		                     auto const          &map_fn)
		{
		        /*
			 * unsupported - deny otherwise arbitrary physical
		         * memory can be mapped to a VM
			 */
		        if (dsc.managed())
				return Attach_result::INVALID_DS;

			if (guest_phys & 0xffful || attr.offset & 0xffful ||
			    attr.size & 0xffful)
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

			using Alloc_error = Range_allocator::Alloc_error;

			Attach_result const retval = _map.alloc_addr(attr.size, guest_phys).convert<Attach_result>(

				[&] (void *) {

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
					try {
						_map.construct_metadata((void *)guest_phys,
						                        dsc, rm_detach, region_attr);

					} catch (Allocator_avl_tpl<Rm_region>::Assign_metadata_failed) {
						if (_remaining_print_count) {
							error("failed to store attachment info");
							_remaining_print_count--;
						}
						return Attach_result::INVALID_DS;
					}

					Rm_region &region = *_map.metadata((void *)guest_phys);

					/* inform dataspace about attachment */
					dsc.attached_to(region);

					return Attach_result::OK;
				},

				[&] (Alloc_error error) {

					switch (error) {

					case Alloc_error::OUT_OF_RAM:
						return Attach_result::OUT_OF_RAM;
					case Alloc_error::OUT_OF_CAPS:
						return Attach_result::OUT_OF_CAPS;
					case Alloc_error::DENIED:
						{
							/*
							 * Handle attach after partial detach
							 */
							Rm_region *region_ptr = _map.metadata((void *)guest_phys);
							if (!region_ptr)
								return Attach_result::REGION_CONFLICT;

							Rm_region &region = *region_ptr;

							bool conflict = false;
							region.with_dataspace([&] (Dataspace_component &dataspace) {
									(void)dataspace;
									if (!(dsc.cap() == dataspace.cap()))
										conflict = true;
							});
							if (conflict)
								return Attach_result::REGION_CONFLICT;

							if (guest_phys < region.base() ||
							    guest_phys > region.base() + region.size() - 1)
								return Attach_result::REGION_CONFLICT;
						}

					};

					return Attach_result::OK;
				}
			);

			if (retval == Attach_result::OK) {
				addr_t phys_addr = dsc.phys_addr() + attr.offset;
				size_t size = attr.size;

				map_fn(guest_phys, phys_addr, size);
			}

			return retval;
		}


		void detach(addr_t     guest_phys,
		            size_t     size,
		            auto const &unmap_fn)
		{
			if (!size || (guest_phys & 0xffful) || (size & 0xffful)) {
				if (_remaining_print_count) {
					warning("vm_session: skipping invalid memory detach addr=",
					        (void *)guest_phys, " size=", (void *)size);
					_remaining_print_count--;
				}
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
					detach_at(region->base(), unmap_fn);
				}

				if (addr >= guest_phys_end - (iteration_size - 1))
					break;

				addr += iteration_size;
			} while (true);
		}


		Guest_memory(Constrained_ram_allocator &constrained_md_ram_alloc,
		             Region_map                &region_map)
		:
			_sliced_heap(constrained_md_ram_alloc, region_map)
		{
			/* configure managed VM area */
			_map.add_range(0UL, ~0UL);
		}

		~Guest_memory()
		{
			/* detach all regions */
			while (true) {
				addr_t out_addr = 0;

				if (!_map.any_block_addr(&out_addr))
					break;

				detach_at(out_addr, [](addr_t, size_t) { });
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
