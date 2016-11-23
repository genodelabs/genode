/*
 * \brief  Core implementation of the RAM session interface
 * \author Norman Feske
 * \date   2006-05-19
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

	return _ds_ep->apply(ds, lambda);
}


void Ram_session_component::_free_ds(Dataspace_capability ds_cap)
{
	Dataspace_component *ds = nullptr;
	_ds_ep->apply(ds_cap, [&] (Dataspace_component *c)
	{
		if (!c) return;
		if (!c->owner(this)) return;

		ds = c;

		size_t ds_size = ds->size();

		/* tell entry point to forget the dataspace */
		_ds_ep->dissolve(ds);

		/* remove dataspace from all RM sessions */
		ds->detach_from_rm_sessions();

		/* destroy native shared memory representation */
		_revoke_ram_ds(ds);

		/* free physical memory that was backing the dataspace */
		_ram_alloc->free((void *)ds->phys_addr(), ds_size);

		/* adjust payload */
		Lock::Guard lock_guard(_ref_members_lock);
		_payload -= ds_size;
	});

	/* call dataspace destructors and free memory */
	if (ds)
		destroy(&_ds_slab, ds);
}


int Ram_session_component::_transfer_quota(Ram_session_component *dst, size_t amount)
{
	/* check if recipient is a valid Ram_session_component */
	if (!dst) return -1;

	/* check for reference account relationship */
	if ((ref_account() != dst) && (dst->ref_account() != this))
		return -2;

	/* decrease quota limit of this session - check against used quota */
	if (_quota_limit < amount + _payload) {
		warning("insufficient quota for transfer: "
		        "'", Cstring(_label), "' to '", Cstring(dst->_label), "' "
		        "have ", (_quota_limit - _payload)/1024, " KiB, "
		        "need ", amount/1024, " KiB");
		return -3;
	}

	_quota_limit -= amount;

	/* increase quota_limit of recipient */
	dst->_quota_limit += amount;

	return 0;
}


void Ram_session_component::_register_ref_account_member(Ram_session_component *new_member)
{
	Lock::Guard lock_guard(_ref_members_lock);
	_ref_members.insert(new_member);
	new_member->_ref_account = this;
}


void Ram_session_component::_unsynchronized_remove_ref_account_member(Ram_session_component *member)
{
	member->_ref_account = 0;
	_ref_members.remove(member);
}


void Ram_session_component::_remove_ref_account_member(Ram_session_component *member)
{
	Lock::Guard lock_guard(_ref_members_lock);
	_unsynchronized_remove_ref_account_member(member);
}


Ram_dataspace_capability Ram_session_component::alloc(size_t ds_size, Cache_attribute cached)
{
	/* zero-sized dataspaces are not allowed */
	if (!ds_size) return Ram_dataspace_capability();

	/* dataspace allocation granularity is page size */
	ds_size = align_addr(ds_size, 12);

	/*
	 * Check quota!
	 *
	 * In the worst case, we need to allocate a new slab block for the
	 * meta data of the dataspace to be created - therefore, we add
	 * the slab block size here.
	 */
	if (used_quota() + SBS + ds_size > _quota_limit)
		throw Quota_exceeded();

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
	if (_phys_start == 0 && _phys_end == ~0UL) {
		addr_t const high_start = (sizeof(void *) == 4 ? 3UL : 4UL) << 30;
		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			if (_ram_alloc->alloc_aligned(ds_size, &ds_addr, align_log2,
			                              high_start, _phys_end).ok()) {
				alloc_succeeded = true;
				break;
			}
		}
	}

	/* apply constraints or re-try because higher memory allocation failed */
	if (!alloc_succeeded) {
		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			if (_ram_alloc->alloc_aligned(ds_size, &ds_addr, align_log2,
			                              _phys_start, _phys_end).ok()) {
				alloc_succeeded = true;
				break;
			}
		}
	}

	/*
	 * Normally, init's quota equals the size of physical memory and this quota
	 * is distributed among the processes. As we check the quota before
	 * allocating, the allocation should always succeed in theory. However,
	 * fragmentation could cause a failing allocation.
	 */
	if (!alloc_succeeded) {
		error("out of physical memory while allocating ", ds_size, " bytes ",
		      "in range [", Hex(_phys_start), "-", Hex(_phys_end), "] - label ",
		      Cstring(_label));
		throw Quota_exceeded();
	}

	Dataspace_component *ds;
	try {
		/*
		 * For non-cached RAM dataspaces, we mark the dataspace as write
		 * combined and expect the pager to evaluate this dataspace property
		 * when resolving page faults.
		 */
		ds = new (&_ds_slab)
			Dataspace_component(ds_size, (addr_t)ds_addr, cached, true, this);
	} catch (Allocator::Out_of_memory) {
		warning("could not allocate metadata");
		/* cleanup unneeded resources */
		_ram_alloc->free(ds_addr);

		throw Out_of_metadata();
	}

	/* create native shared memory representation of dataspace */
	try {
		_export_ram_ds(ds);
	} catch (Out_of_metadata) {
		warning("could not export RAM dataspace of size ", ds->size());
		/* cleanup unneeded resources */
		destroy(&_ds_slab, ds);
		_ram_alloc->free(ds_addr);

		throw Quota_exceeded();
	}

	/*
	 * Fill new dataspaces with zeros. For non-cached RAM dataspaces, this
	 * function must also make sure to flush all cache lines related to the
	 * address range used by the dataspace.
	 */
	_clear_ds(ds);

	Dataspace_capability result = _ds_ep->manage(ds);

	Lock::Guard lock_guard(_ref_members_lock);
	/* keep track of the used quota for actual payload */
	_payload += ds_size;

	return static_cap_cast<Ram_dataspace>(result);
}


void Ram_session_component::free(Ram_dataspace_capability ds_cap) {
	_free_ds(ds_cap); }


int Ram_session_component::ref_account(Ram_session_capability ram_session_cap)
{
	/* the reference account cannot be defined twice */
	if (_ref_account) return -2;

	auto lambda = [this] (Ram_session_component *ref) {

		/* check if recipient is a valid Ram_session_component */
		if (!ref) return -1;

		/* deny the usage of the ram session as its own ref account */
		/* XXX also check for cycles along the tree of ref accounts */
		if (ref == this) return -3;

		_ref_account = ref;
		_ref_account->_register_ref_account_member(this);
		return 0;
	};

	return _ram_session_ep->apply(ram_session_cap, lambda);
}


int Ram_session_component::transfer_quota(Ram_session_capability ram_session_cap,
                                          size_t amount)
{
	auto lambda = [&] (Ram_session_component *dst) {
		return _transfer_quota(dst, amount); };

	if (this->cap() == ram_session_cap)
		return 0;

	return _ram_session_ep->apply(ram_session_cap, lambda);
}


Ram_session_component::Ram_session_component(Rpc_entrypoint  *ds_ep,
                                             Rpc_entrypoint  *ram_session_ep,
                                             Range_allocator *ram_alloc,
                                             Allocator       *md_alloc,
                                             const char      *args,
                                             size_t           quota_limit)
:
	_ds_ep(ds_ep), _ram_session_ep(ram_session_ep), _ram_alloc(ram_alloc),
	_quota_limit(quota_limit), _payload(0),
	_md_alloc(md_alloc, Arg_string::find_arg(args, "ram_quota").ulong_value(0)),
	_ds_slab(&_md_alloc), _ref_account(0),
	_phys_start(Arg_string::find_arg(args, "phys_start").ulong_value(0))
{
	Arg_string::find_arg(args, "label").string(_label, sizeof(_label), "");

	size_t phys_size = Arg_string::find_arg(args, "phys_size").ulong_value(0);
	/* sanitize overflow and interpret phys_size==0 as maximum phys address */
	if (_phys_start + phys_size <= _phys_start)
		_phys_end = ~0UL;
	else
		_phys_end = _phys_start + phys_size - 1;
}


Ram_session_component::~Ram_session_component()
{
	/* destroy all dataspaces */
	for (Dataspace_component *ds; (ds = _ds_slab()->first_object());
	     _free_ds(ds->cap()));

	if (_payload != 0)
		warning("remaining payload of ", _payload, " in ram session to destroy");

	if (!_ref_account) return;

	/* transfer remaining quota to reference account */
	_transfer_quota(_ref_account, _quota_limit);

	/* remember our original reference account */
	Ram_session_component *orig_ref_account = _ref_account;

	/* remove reference to us from the reference account */
	_ref_account->_remove_ref_account_member(this);

	/*
	 * Now, the '_ref_account' member has become invalid.
	 */

	Lock::Guard lock_guard(_ref_members_lock);

	/* assign all sub accounts to our original reference account */
	for (Ram_session_component *rsc; (rsc = _ref_members.first()); ) {

		_unsynchronized_remove_ref_account_member(rsc);

		/*
		 * This function grabs the '_ref_account_lock' of the '_ref_account',
		 * which is never identical to ourself. Hence, deadlock cannot happen
		 * here.
		 */
		orig_ref_account->_register_ref_account_member(rsc);
	}

	_ref_account = 0;
}
