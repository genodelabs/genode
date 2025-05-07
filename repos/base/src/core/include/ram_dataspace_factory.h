/*
 * \brief  RAM dataspace factory
 * \author Norman Feske
 * \date   2017-05-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RAM_DATASPACE_FACTORY_H_
#define _CORE__INCLUDE__RAM_DATASPACE_FACTORY_H_

/* Genode includes */
#include <base/heap.h>
#include <base/tslab.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* core includes */
#include <dataspace_component.h>

namespace Core { class Ram_dataspace_factory; }


class Core::Ram_dataspace_factory : public Dataspace_owner
{
	public:

		using Phys_range = Range_allocator::Range;

		static Phys_range any_phys_range() { return { 0UL, ~0UL }; }

		struct Virt_range { addr_t start, size; };

		static constexpr size_t SLAB_BLOCK_SIZE = 4096;

	private:

		Rpc_entrypoint &_ep;

		Range_allocator &_phys_alloc;
		Phys_range const _phys_range;


		/*
		 * Statically allocated initial slab block for '_ds_slab', needed to
		 * untangle the hen-and-egg problem of allocating the meta data for
		 * core's RAM allocator from itself. I also saves the allocation
		 * of one dataspace (along with a dataspace capability) per session.
		 */
		uint8_t _initial_sb[SLAB_BLOCK_SIZE];

		Tslab<Dataspace_component, SLAB_BLOCK_SIZE> _ds_slab;


		/********************************************
		 ** Platform-implemented support functions **
		 ********************************************/

		/**
		 * Export RAM dataspace as shared memory block
		 *
		 * \return false if core-virtual memory is exhausted
		 */
		[[nodiscard]] bool _export_ram_ds(Dataspace_component &);

		/**
		 * Revert export of RAM dataspace
		 */
		void _revoke_ram_ds(Dataspace_component &ds);

		/**
		 * Zero-out content of dataspace
		 */
		void _clear_ds(Dataspace_component &ds);

	public:

		using Alloc_ram_error  = Pd_session::Alloc_ram_error;
		using Alloc_ram_result = Pd_session::Alloc_ram_result;

		Ram_dataspace_factory(Rpc_entrypoint  &ep,
		                      Range_allocator &phys_alloc,
		                      Phys_range       phys_range,
		                      Allocator       &allocator)
		:
			_ep(ep), _phys_alloc(phys_alloc), _phys_range(phys_range),
			_ds_slab(allocator, _initial_sb)
		{ }

		~Ram_dataspace_factory()
		{
			while (Dataspace_component *ds = _ds_slab.first_object())
				free_ram(static_cap_cast<Ram_dataspace>(
				       static_cap_cast<Dataspace>(ds->cap())));
		}

		addr_t dataspace_dma_addr(Ram_dataspace_capability);

		Alloc_ram_result alloc_ram(size_t, Cache);
		void free_ram(Ram_dataspace_capability);
		size_t ram_size(Ram_dataspace_capability ds);
};

#endif /* _CORE__INCLUDE__RAM_DATASPACE_FACTORY_H_ */
