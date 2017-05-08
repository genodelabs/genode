/*
 * \brief  Core-specific instance of the RAM session interface
 * \author Norman Feske
 * \date   2006-06-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/heap.h>
#include <base/tslab.h>
#include <base/session_object.h>
#include <base/allocator_guard.h>
#include <base/synced_allocator.h>
#include <base/session_label.h>

/* core includes */
#include <dataspace_component.h>
#include <util.h>
#include <account.h>

namespace Genode { class Ram_session_component; }


class Genode::Ram_session_component : public Session_object<Ram_session>,
                                      public Dataspace_owner
{
	public:

		struct Phys_range { addr_t start, end; };

		static Phys_range any_phys_range() { return { 0UL, ~0UL }; }

	private:

		class Invalid_dataspace : public Exception { };

		/*
		 * Dimension 'Ds_slab' such that slab blocks (including the
		 * meta-data overhead of the sliced-heap blocks) are page sized.
		 */
		static constexpr size_t SBS = get_page_size() - Sliced_heap::meta_data_size();

		using Ds_slab = Tslab<Dataspace_component, SBS>;

		Rpc_entrypoint &_ep;

		Range_allocator &_phys_alloc;

		Constrained_ram_allocator _constrained_md_ram_alloc;

		Constructible<Sliced_heap> _sliced_heap;

		/*
		 * Statically allocated initial slab block for '_ds_slab', needed to
		 * untangle the hen-and-egg problem of allocating the meta data for
		 * core's RAM allocator from itself. I also saves the allocation
		 * of one dataspace (along with a dataspace capability) per session.
		 */
		uint8_t _initial_sb[SBS];

		Constructible<Ds_slab> _ds_slab;

		Phys_range const _phys_range;

		Constructible<Account<Ram_quota> > _ram_account;

		/**
		 * Free dataspace
		 */
		void _free_ds(Dataspace_capability ds_cap);


		/********************************************
		 ** Platform-implemented support functions **
		 ********************************************/

		struct Core_virtual_memory_exhausted : Exception { };

		/**
		 * Export RAM dataspace as shared memory block
		 *
		 * \throw Core_virtual_memory_exhausted
		 */
		void _export_ram_ds(Dataspace_component *ds);

		/**
		 * Revert export of RAM dataspace
		 */
		void _revoke_ram_ds(Dataspace_component *ds);

		/**
		 * Zero-out content of dataspace
		 */
		void _clear_ds(Dataspace_component *ds);

	public:

		Ram_session_component(Rpc_entrypoint      &ep,
		                      Resources            resources,
		                      Session_label const &label,
		                      Diag                 diag,
		                      Range_allocator     &phys_alloc,
		                      Region_map          &local_rm,
		                      Phys_range           phys_range);

		~Ram_session_component();

		/**
		 * Initialize RAM account without providing a reference account
		 *
		 * This method is solely used to set up the initial RAM session within
		 * core. The RAM accounts of regular RAM session are initialized via
		 * 'ref_account'.
		 */
		void init_ram_account() { _ram_account.construct(*this, _label); }

		/**
		 * Get physical address of the RAM that backs a dataspace
		 *
		 * \param ds  targeted dataspace
		 *
		 * \throw Invalid_dataspace
		 */
		addr_t phys_addr(Ram_dataspace_capability ds);


		/*****************************
		 ** Ram_allocator interface **
		 *****************************/

		Ram_dataspace_capability alloc(size_t, Cache_attribute) override;

		void free(Ram_dataspace_capability) override;

		size_t dataspace_size(Ram_dataspace_capability ds) const override;


		/***************************
		 ** RAM Session interface **
		 ***************************/

		void ref_account(Ram_session_capability) override;

		void transfer_quota(Ram_session_capability, Ram_quota) override;

		Ram_quota ram_quota() const override
		{
			return _ram_account.constructed() ? _ram_account->limit() : Ram_quota { 0 };
		}

		Ram_quota used_ram() const override
		{
			return _ram_account.constructed() ? _ram_account->used() : Ram_quota { 0 };
		}
};

#endif /* _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_ */
