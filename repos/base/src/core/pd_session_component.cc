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


Ram_allocator::Alloc_result
Pd_session_component::try_alloc(size_t ds_size, Cache cache)
{
	/* zero-sized dataspaces are not allowed */
	if (!ds_size)
		return Alloc_error::DENIED;

	/* dataspace allocation granularity is page size */
	ds_size = align_addr(ds_size, 12);

	using Result      = Ram_allocator::Alloc_result;
	using Reservation = Genode::Reservation;

	/* track quota use */
	return _ram_quota_guard().with_reservation<Result>(Ram_quota{ds_size},

		[&] (Reservation &ram_reservation) -> Result {

			/*
			 * In the worst case, we need to allocate a new slab block for
			 * the meta data of the dataspace to be created. Therefore, we
			 * temporarily withdraw the slab block size here to trigger an
			 * exception if the account does not have enough room for the meta
			 * data.
			 */
			Ram_quota const overhead { Ram_dataspace_factory::SLAB_BLOCK_SIZE };

			if (!_ram_quota_guard().have_avail(overhead)) {
				ram_reservation.cancel();
				return Ram_allocator::Alloc_error::OUT_OF_RAM;
			}

			/*
			 * Each dataspace is an RPC object and thereby consumes a
			 * capability.
			 */
			return _cap_quota_guard().with_reservation<Result>(Cap_quota{1},

				[&] (Genode::Reservation &) -> Result {
					return _ram_ds_factory.try_alloc(ds_size, cache);
				},
				[&] () -> Result {
					ram_reservation.cancel();
					return Ram_allocator::Alloc_error::OUT_OF_CAPS;
				}
			);
		},
		[&] () -> Result {
			return Ram_allocator::Alloc_error::OUT_OF_RAM;
		}
	);
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
			throw Out_of_ram(); }
	});
}


addr_t Pd_session_component::dma_addr(Ram_dataspace_capability ds_cap)
{
	if (_managing_system == Managing_system::DENIED)
		return 0;

	if (this->cap() == ds_cap)
		return 0;

	return _ram_ds_factory.dataspace_dma_addr(ds_cap);
}


Pd_session::Attach_dma_result
Pd_session_component::attach_dma(Dataspace_capability ds_cap, addr_t at)
{
	if (_managing_system == Managing_system::DENIED)
		return Attach_dma_error::DENIED;

	if (this->cap() == ds_cap)
		return Attach_dma_error::DENIED;

	return _address_space.attach_dma(ds_cap, at);
}
