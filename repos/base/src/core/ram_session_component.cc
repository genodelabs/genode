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

/* core includes */
#include <ram_session_component.h>

using namespace Genode;


Ram_dataspace_capability
Ram_session_component::alloc(size_t ds_size, Cache_attribute cached)
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
		Ram_quota const overhead { Ram_dataspace_factory::SLAB_BLOCK_SIZE };
		Ram_quota_guard::Reservation sbs_ram_costs(*this, overhead);
	}

	/*
	 * Each dataspace is an RPC object and thereby consumes a capability.
	 */
	Cap_quota_guard::Reservation dataspace_cap_costs(*this, Cap_quota{1});

	/*
	 * Allocate physical dataspace
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 */
	Ram_dataspace_capability ram_ds = _ram_ds_factory.alloc(ds_size, cached);

	/*
	 * We returned from '_ram_ds_factory.alloc' with a valid dataspace.
	 */
	dataspace_ram_costs.acknowledge();
	dataspace_cap_costs.acknowledge();

	return ram_ds;
}


void Ram_session_component::free(Ram_dataspace_capability ds_cap)
{
	if (this->cap() == ds_cap)
		return;

	size_t const size = _ram_ds_factory.dataspace_size(ds_cap);
	if (size == 0)
		return;

	_ram_ds_factory.free(ds_cap);

	/* physical memory */
	_ram_account->replenish(Ram_quota{size});

	/* capability of the dataspace RPC object */
	Cap_quota_guard::replenish(Cap_quota{1});
}


size_t Ram_session_component::dataspace_size(Ram_dataspace_capability ds_cap) const
{
	if (this->cap() == ds_cap)
		return 0;

	return _ram_ds_factory.dataspace_size(ds_cap);
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
	_constrained_md_ram_alloc(*this, *this, *this),
	_sliced_heap(_constrained_md_ram_alloc, local_rm),
	_ram_ds_factory(ep, phys_alloc, phys_range, local_rm, _sliced_heap)
{ }
