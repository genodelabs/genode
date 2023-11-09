/*
 * \brief  Internal acpi io memory management
 * \author Alexander Boettcher
 * \date   2015-02-16
 */

 /*
  * Copyright (C) 2015-2017 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <base/allocator.h>
#include <base/allocator_avl.h>
#include <base/env.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

namespace Acpi {
	using namespace Genode;

	class Memory;
}

class Acpi::Memory
{
	public:

		struct Unsupported_range { };

	private:

		/*
		 * We wrap the connection into Constructible to prevent a "accessible
		 * non-virtual destructor" compiler error with Allocator_avl_base::Block.
		 */
		struct Io_mem
		{
			struct Region
			{
				addr_t _base;
				size_t _size;

				static addr_t _base_align(addr_t base)
				{
					return base & ~0xfffUL;
				}

				static addr_t _size_align(addr_t base, size_t size)
				{
					return align_addr(base + (size - 1) - _base_align(base), 12);
				}

				Region(addr_t base, size_t size)
				:
					_base(_base_align(base)),
					_size(_size_align(base, size))
				{ }

				addr_t base() const { return _base; }
				addr_t last() const { return _base + (_size - 1); }
				size_t size() const { return _size; }

				bool contains(Region const &o) const
				{
					return o.base() >= base() && o.last() <= last();
				}

				void print(Output &o) const
				{
					Genode::print(o, Hex_range<addr_t>(_base, _size));
				}
			} region;

			Constructible<Io_mem_connection> connection { };

			Io_mem(Env &env, Region region) : region(region)
			{
				connection.construct(env, region.base(), region.size());
			}
		};

		static constexpr unsigned long
			ACPI_REGION_SIZE_LOG2 = 30, /* 1 GiB range */
			ACPI_REGION_SIZE      = 1UL << ACPI_REGION_SIZE_LOG2;

		Env       &_env;
		Allocator &_heap;

		Rm_connection      _rm             { _env };
		Region_map_client  _acpi_window    { _rm.create(ACPI_REGION_SIZE) };
		Attached_dataspace _acpi_window_ds { _env.rm(), _acpi_window.dataspace() };
		addr_t       const _acpi_base      { (addr_t)_acpi_window_ds.local_addr<void>()};

		Constructible<Io_mem::Region> _io_region { };

		addr_t _acpi_ptr(addr_t base) const
		{
			/* virtual address inside the mapped ACPI window */
			return _acpi_base + (base - _io_region->base());
		}

		Allocator_avl_tpl<Io_mem> _range { &_heap };

	public:

		Memory(Env &env, Allocator &heap) : _env(env), _heap(heap)
		{
			_range.add_range(0, ~0UL);
		}

		addr_t map_region(addr_t const req_base, addr_t const req_size)
		{
			/*
			 * The first caller sets the upper physical bits of addresses and,
			 * thereby, determines the valid range of addresses.
			 */

			if (!_io_region.constructed()) {
				_io_region.construct(req_base & _align_mask(ACPI_REGION_SIZE_LOG2),
				                     ACPI_REGION_SIZE);
			}

			/* requested region of I/O memory */
			Io_mem::Region loop_region { req_base, req_size };

			/* check that physical region fits into supported range */
			if (!_io_region->contains(loop_region)) {
				warning("acpi table out of range - ", loop_region, " not in ", *_io_region);
				throw Unsupported_range();
			}

			/* early return if the region is already mapped */
			if (Io_mem *m = _range.metadata((void *)req_base)) {
				if (m->region.contains(loop_region)) {
					return _acpi_ptr(req_base);
				}
			}

			/*
			 * We iterate over the requested region looking for collisions with
			 * existing mappings. On a collision, we extend the requested range
			 * to comprise also the existing mapping and destroy the mapping.
			 * Finally, we request the compound region as on I/O memory
			 * mapping.
			 *
			 * Note, this approach unfortunately does not merge consecutive
			 * regions.
			 */

			addr_t loop_offset = 0;
			while (loop_offset < loop_region.size()) {
				void * const addr = (void *)(loop_region.base() + loop_offset);

				if (Io_mem *m = _range.metadata(addr)) {
					addr_t const region_base = m->region.base();
					addr_t const region_size = m->region.size();
					addr_t const compound_base = min(loop_region.base(), region_base);
					addr_t const compound_end  = max(loop_region.base() + loop_region.size(),
					                                 region_base + region_size);

					_acpi_window.detach(region_base - _io_region->base());
					m->~Io_mem();
					_range.free((void *)region_base);

					/* now start over */
					loop_region = Io_mem::Region(compound_base, compound_end - compound_base);
					loop_offset = 0;
				}

				loop_offset += 0x1000;
			}

			/* allocate ACPI range as I/O memory */
			_range.alloc_addr(loop_region.size(), loop_region.base());
			_range.construct_metadata((void *)loop_region.base(), _env, loop_region);

			/*
			 * We attach the I/O memory dataspace into a virtual-memory window,
			 * which starts at _io_region.base(). Therefore, the attachment
			 * address is the offset of loop_region.base() from
			 * _io_region.base().
			 */
			_acpi_window.attach_at(
				_range.metadata((void *)loop_region.base())->connection->dataspace(),
				loop_region.base() - _io_region->base(), loop_region.size());

			return _acpi_ptr(req_base);
		}

		void free_io_memory()
		{
			addr_t out_addr = 0;
			while (_range.any_block_addr(&out_addr)) {
				_acpi_window.detach(out_addr - _io_region->base());
				_range.metadata((void *)out_addr)->~Io_mem();
				_range.free((void *)out_addr);
			}
		}
};

#endif /* _MEMORY_H_ */
