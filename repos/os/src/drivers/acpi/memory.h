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

namespace Acpi { class Memory; }

class Acpi::Memory
{
	private:

		class Io_mem : public Genode::List<Io_mem>::Element
		{
			private:
				Genode::Io_mem_connection _io_mem;

			public:
				Io_mem(Genode::Env &env, Genode::addr_t phys)
				: _io_mem(env, phys, 0x1000UL) { }

				Genode::Io_mem_dataspace_capability dataspace()
				{
					return _io_mem.dataspace();
				}
		};

		Genode::Env              &_env;
		Genode::addr_t      const ACPI_REGION_SIZE_LOG2;
		Genode::Rm_connection     _rm;
		Genode::Region_map_client _rm_acpi;
		Genode::addr_t      const _acpi_base;
		Genode::Allocator        &_heap;
		Genode::Allocator_avl     _range;
		Genode::List<Io_mem>      _io_mem_list { };

	public:

		Memory(Genode::Env &env, Genode::Allocator &heap)
		:
			_env(env),
			/* 1 GB range */
			ACPI_REGION_SIZE_LOG2(30),
			_rm(env),
			_rm_acpi(_rm.create(1UL << ACPI_REGION_SIZE_LOG2)),
			_acpi_base(env.rm().attach(_rm_acpi.dataspace())),
			_heap(heap),
			_range(&_heap)
		{
			_range.add_range(0, 1UL << ACPI_REGION_SIZE_LOG2);
		}

		Genode::addr_t phys_to_virt(Genode::addr_t const phys, Genode::addr_t const p_size)
		{
			using namespace Genode;

			/* the first caller sets the upper physical bits of addresses */
			static addr_t const high = phys & _align_mask(ACPI_REGION_SIZE_LOG2);

			/* sanity check that physical address is in range we support */
			if ((phys & _align_mask(ACPI_REGION_SIZE_LOG2)) != high) {
				addr_t const end = high + (1UL << ACPI_REGION_SIZE_LOG2) - 1;
				error("acpi table out of range - ", Hex(phys), " "
				      "not in ", Hex_range<addr_t>(high, end - high));
				throw -1;
			}

			addr_t const phys_aligned = phys & _align_mask(12);
			addr_t const size_aligned = align_addr(p_size + (phys & _align_offset(12)), 12);

			for (addr_t size = 0; size < size_aligned; size += 0x1000UL) {
				addr_t const low = (phys_aligned + size) &
				                     _align_offset(ACPI_REGION_SIZE_LOG2);
				if (!_range.alloc_addr(0x1000UL, low).ok())
					continue;

				/* allocate acpi page as io memory */
				Io_mem *mem = new (_heap) Io_mem(_env, phys_aligned + size);
				/* attach acpi page to this process */
				_rm_acpi.attach_at(mem->dataspace(), low, 0x1000UL);
				/* add to list to free when parsing acpi table is done */
				_io_mem_list.insert(mem);
			}

			return _acpi_base + (phys & _align_offset(ACPI_REGION_SIZE_LOG2));
		}

		void free_io_memory()
		{
			while (Io_mem * io_mem = _io_mem_list.first()) {
				_io_mem_list.remove(io_mem);
				destroy(_heap, io_mem);
			}

			Genode::addr_t out_addr;
			while (_range.any_block_addr(&out_addr))
				_range.free((void *)out_addr);
		}
};

#endif /* _MEMORY_H_ */
