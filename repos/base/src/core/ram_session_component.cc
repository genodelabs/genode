/*
 * \brief  Core implementation of the RAM session interface
 * \author Norman Feske
 * \date   2006-05-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/arg_string.h>

/* core includes */
#include <ram_session_component.h>

using namespace Genode;


addr_t Ram_session_component::phys_addr(Ram_dataspace_capability ds)
{
	auto lambda = [] (Dataspace_component *dsc) {
		if (!dsc) throw Invalid_dataspace();
		return dsc->phys_addr();
	};

	return _ep.apply(ds, lambda);
}


void Ram_session_component::_free_ds(Dataspace_capability ds_cap)
{
	Dataspace_component *ds = nullptr;
	_ep.apply(ds_cap, [&] (Dataspace_component *c)
	{
		if (!c) return;
		if (!c->owner(this)) return;

		ds = c;

		size_t ds_size = ds->size();

		/* tell entry point to forget the dataspace */
		_ep.dissolve(ds);

		/* remove dataspace from all RM sessions */
		ds->detach_from_rm_sessions();

		/* destroy native shared memory representation */
		_revoke_ram_ds(ds);

		/* free physical memory that was backing the dataspace */
		_phys_alloc.free((void *)ds->phys_addr(), ds_size);

		_ram_account->replenish(Ram_quota{ds_size});
	});

	/* call dataspace destructors and free memory */
	if (ds) {
		destroy(*_ds_slab, ds);
		Cap_quota_guard::replenish(Cap_quota{1});
	}
}


Ram_dataspace_capability Ram_session_component::alloc(size_t ds_size, Cache_attribute cached)
{
	/* zero-sized dataspaces are not allowed */
	if (!ds_size) return Ram_dataspace_capability();

	/* dataspace allocation granularity is page size */
	ds_size = align_addr(ds_size, 12);

	/*
	 * Track quota usage
	 *
	 * We use a guard to roll back the withdrawal of the quota whenever
	 * we leave the method scope via an exception. The withdrawal is
	 * acknowledge just before successfully leaving the method.
	 */
	Ram_quota_guard::Reservation dataspace_ram_costs(*this, Ram_quota{ds_size});

	/*
	 * In the worst case, we need to allocate a new slab block for the
	 * meta data of the dataspace to be created. Therefore, we temporarily
	 * withdraw the slab block size here to trigger an exception if the
	 * account does not have enough room for the meta data.
	 */
	{
		Ram_quota_guard::Reservation sbs_ram_costs(*this, Ram_quota{SBS});
	}

	/*
	 * Each dataspace is an RPC object and thereby consumes a capability.
	 */
	Cap_quota_guard::Reservation dataspace_cap_costs(*this, Cap_quota{1});

	/*
	 * Allocate physical backing store
	 *
	 * As an optimization for the use of large mapping sizes, we try to
	 * align the dataspace in physical memory naturally (size-aligned).
	 * If this does not work, we subsequently weaken the alignment constraint
	 * until the allocation succeeds.
	 */
	void *ds_addr = 0;
	bool alloc_succeeded = false;

	/*
	 * If no physical constraint exists, try to allocate physical memory at
	 * high locations (3G for 32-bit / 4G for 64-bit platforms) in order to
	 * preserve lower physical regions for device drivers, which may have DMA
	 * constraints.
	 */
	if (_phys_range.start == 0 && _phys_range.end == ~0UL) {
		addr_t const high_start = (sizeof(void *) == 4 ? 3UL : 4UL) << 30;
		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			if (_phys_alloc.alloc_aligned(ds_size, &ds_addr, align_log2,
			                              high_start, _phys_range.end).ok()) {
				alloc_succeeded = true;
				break;
			}
		}
	}

	/* apply constraints or re-try because higher memory allocation failed */
	if (!alloc_succeeded) {
		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			if (_phys_alloc.alloc_aligned(ds_size, &ds_addr, align_log2,
			                              _phys_range.start, _phys_range.end).ok()) {
				alloc_succeeded = true;
				break;
			}
		}
	}

	/*
	 * Helper to release the allocated physical memory whenever we leave the
	 * scope via an exception.
	 */
	struct Phys_alloc_guard
	{
		Range_allocator &phys_alloc;
		void * const ds_addr;
		bool ack = false;

		Phys_alloc_guard(Range_allocator &phys_alloc, void *ds_addr)
		: phys_alloc(phys_alloc), ds_addr(ds_addr) { }

		~Phys_alloc_guard() { if (!ack) phys_alloc.free(ds_addr); }

	} phys_alloc_guard(_phys_alloc, ds_addr);

	/*
	 * Normally, init's quota equals the size of physical memory and this quota
	 * is distributed among the processes. As we check the quota before
	 * allocating, the allocation should always succeed in theory. However,
	 * fragmentation could cause a failing allocation.
	 */
	if (!alloc_succeeded) {
		error("out of physical memory while allocating ", ds_size, " bytes ",
		      "in range [", Hex(_phys_range.start), "-", Hex(_phys_range.end), "]");
		throw Out_of_ram();
	}

	/*
	 * For non-cached RAM dataspaces, we mark the dataspace as write
	 * combined and expect the pager to evaluate this dataspace property
	 * when resolving page faults.
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 */
	Dataspace_component *ds = new (*_ds_slab)
		Dataspace_component(ds_size, (addr_t)ds_addr, cached, true, this);

	/* create native shared memory representation of dataspace */
	try { _export_ram_ds(ds); }
	catch (Core_virtual_memory_exhausted) {
		warning("could not export RAM dataspace of size ", ds->size());

		/* cleanup unneeded resources */
		destroy(*_ds_slab, ds);
		throw Out_of_ram();
	}

	/*
	 * Fill new dataspaces with zeros. For non-cached RAM dataspaces, this
	 * function must also make sure to flush all cache lines related to the
	 * address range used by the dataspace.
	 */
	_clear_ds(ds);

	Dataspace_capability result = _ep.manage(ds);

	dataspace_ram_costs.acknowledge();
	dataspace_cap_costs.acknowledge();
	phys_alloc_guard.ack = true;

	return static_cap_cast<Ram_dataspace>(result);
}


void Ram_session_component::free(Ram_dataspace_capability ds_cap)
{
	_free_ds(ds_cap);
}


size_t Ram_session_component::dataspace_size(Ram_dataspace_capability ds_cap) const
{
	if (this->cap() == ds_cap)
		return 0;

	size_t result = 0;
	_ep.apply(ds_cap, [&] (Dataspace_component *c) {
		if (c && c->owner(this))
			result = c->size(); });

	return result;
}


void Ram_session_component::ref_account(Ram_session_capability ram_session_cap)
{
	/* the reference account can be defined only once */
	if (_ram_account.constructed())
		return;

	if (this->cap() == ram_session_cap)
		return;

	_ep.apply(ram_session_cap, [&] (Ram_session_component *ram) {

		if (!ram || !ram->_ram_account.constructed()) {
			error("invalid RAM session specified as ref account");
			throw Invalid_session();
		}

		_ram_account.construct(*this, _label, *ram->_ram_account);
	});
}


void Ram_session_component::transfer_quota(Ram_session_capability ram_session_cap,
                                           Ram_quota amount)
{
	/* the reference account can be defined only once */
	if (!_ram_account.constructed())
		throw Undefined_ref_account();

	if (this->cap() == ram_session_cap)
		return;

	_ep.apply(ram_session_cap, [&] (Ram_session_component *ram) {

		if (!ram || !ram->_ram_account.constructed())
			throw Invalid_session();

		try {
			_ram_account->transfer_quota(*ram->_ram_account, amount); }
		catch (Account<Ram_quota>::Unrelated_account) {
			warning("attempt to transfer RAM quota to unrelated RAM session");
			throw Invalid_session(); }
		catch (Account<Ram_quota>::Limit_exceeded) {
			warning("RAM limit (", *_ram_account, ") exceeded "
			        "during transfer_quota(", amount, ")");
			throw Out_of_ram(); }
	});
}


Ram_session_component::Ram_session_component(Rpc_entrypoint  &ep,
                                             Resources        resources,
                                             Label     const &label,
                                             Diag             diag,
                                             Range_allocator &phys_alloc,
                                             Region_map      &local_rm,
                                             Phys_range       phys_range)
:
	Session_object(ep, resources, label, diag),
	_ep(ep),
	_phys_alloc(phys_alloc),
	_constrained_md_ram_alloc(*this, *this, *this),
	_phys_range(phys_range)
{
	_sliced_heap.construct(_constrained_md_ram_alloc, local_rm);
	_ds_slab.construct(*_sliced_heap, _initial_sb);
}


Ram_session_component::~Ram_session_component()
{
	/* destroy all dataspaces */
	Ds_slab &ds_slab = *_ds_slab;
	for (Dataspace_component *ds; (ds = ds_slab.first_object());
	     _free_ds(ds->cap()));
}
