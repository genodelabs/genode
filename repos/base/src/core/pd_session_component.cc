/*
 * \brief  Core implementation of the PD session interface
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
#include <pd_session_component.h>

using namespace Genode;


Ram_dataspace_capability
Pd_session_component::alloc(size_t ds_size, Cache_attribute cached)
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
	Ram_quota_guard::Reservation
		dataspace_ram_costs(_ram_quota_guard(), Ram_quota{ds_size});

	/*
	 * In the worst case, we need to allocate a new slab block for the
	 * meta data of the dataspace to be created. Therefore, we temporarily
	 * withdraw the slab block size here to trigger an exception if the
	 * account does not have enough room for the meta data.
	 */
	{
		Ram_quota const overhead { Ram_dataspace_factory::SLAB_BLOCK_SIZE };
		Ram_quota_guard::Reservation sbs_ram_costs(_ram_quota_guard(), overhead);
	}

	/*
	 * Each dataspace is an RPC object and thereby consumes a capability.
	 */
	Cap_quota_guard::Reservation
		dataspace_cap_costs(_cap_quota_guard(), Cap_quota{1});

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


void Pd_session_component::free(Ram_dataspace_capability ds_cap)
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
	_released_cap(DS_CAP);
}


size_t Pd_session_component::dataspace_size(Ram_dataspace_capability ds_cap) const
{
	if (this->cap() == ds_cap)
		return 0;

	return _ram_ds_factory.dataspace_size(ds_cap);
}


void Pd_session_component::ref_account(Capability<Pd_session> pd_cap)
{
	/* the reference account can be defined only once */
	if (_cap_account.constructed())
		return;

	if (this->cap() == pd_cap)
		return;

	_ep.apply(pd_cap, [&] (Pd_session_component *pd) {

		if (!pd || !pd->_ram_account.constructed()) {
			error("invalid PD session specified as ref account");
			throw Invalid_session();
		}

		_cap_account.construct(_cap_quota_guard(), _label, *pd->_cap_account);
		_ram_account.construct(_ram_quota_guard(), _label, *pd->_ram_account);
	});
}


void Pd_session_component::transfer_quota(Capability<Pd_session> pd_cap,
                                          Cap_quota amount)
{
	if (!_cap_account.constructed())
		throw Undefined_ref_account();

	if (this->cap() == pd_cap)
		return;

	_ep.apply(pd_cap, [&] (Pd_session_component *pd) {

		if (!pd || !pd->_cap_account.constructed())
			throw Invalid_session();

		try {
			_cap_account->transfer_quota(*pd->_cap_account, amount);
			diag("transferred ", amount, " caps "
			     "to '", pd->_cap_account->label(), "' (", _cap_account, ")");
		}
		catch (Account<Cap_quota>::Unrelated_account) {
			warning("attempt to transfer cap quota to unrelated PD session");
			throw Invalid_session(); }
		catch (Account<Cap_quota>::Limit_exceeded) {
			warning("cap limit (", *_cap_account, ") exceeded "
			        "during transfer_quota(", amount, ")");
			throw Out_of_caps(); }
	});
}


void Pd_session_component::transfer_quota(Capability<Pd_session> pd_cap,
                                          Ram_quota amount)
{
	if (!_ram_account.constructed())
		throw Undefined_ref_account();

	if (this->cap() == pd_cap)
		return;

	_ep.apply(pd_cap, [&] (Pd_session_component *pd) {

		if (!pd || !pd->_ram_account.constructed())
			throw Invalid_session();

		try {
			_ram_account->transfer_quota(*pd->_ram_account, amount); }
		catch (Account<Ram_quota>::Unrelated_account) {
			warning("attempt to transfer RAM quota to unrelated PD session");
			throw Invalid_session(); }
		catch (Account<Ram_quota>::Limit_exceeded) {
			warning("RAM limit (", *_ram_account, ") exceeded "
			        "during transfer_quota(", amount, ")");
			throw Out_of_ram(); }
	});
}

