/*
 * \brief  Core implementation of the RAM session interface
 * \author Norman Feske
 * \date   2006-05-19
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <ram_session_component.h>

using namespace Genode;


static const bool verbose = false;


addr_t Ram_session_component::phys_addr(Ram_dataspace_capability ds)
{
	Dataspace_component * const dsc =
		dynamic_cast<Dataspace_component *>(_ds_ep->obj_by_cap(ds));

	if (!dsc) throw Invalid_dataspace();
	return dsc->phys_addr();
}


void Ram_session_component::_free_ds(Dataspace_component *ds)
{
	if (!ds) return;
	if (!ds->owner(this)) return;

	size_t ds_size = ds->size();

	/* destroy native shared memory representation */
	_revoke_ram_ds(ds);

	/* tell entry point to forget the dataspace */
	_ds_ep->dissolve(ds);

	/* XXX: remove dataspace from all RM sessions */

	/* free physical memory that was backing the dataspace */
	_ram_alloc->free((void *)ds->phys_addr(), ds_size);

	/* call dataspace destructors and free memory */
	destroy(&_ds_slab, ds);

	/* adjust payload */
	_payload -= ds_size;
}


int Ram_session_component::_transfer_quota(Ram_session_component *dst, size_t amount)
{
	/* check if recipient is a valid Ram_session_component */
	if (!dst) return -1;

	/* check for reference account relationship */
	if ((ref_account() != dst) && (dst->ref_account() != this))
		return -3;

	/* decrease quota limit of this session - check against used quota */
	if (_quota_limit < amount + _payload) {
		PWRN("Insufficient quota for transfer: %s", _label);
		PWRN("  have %zd, need %zd", _quota_limit - _payload, amount);
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


Ram_dataspace_capability Ram_session_component::alloc(size_t ds_size, bool cached)
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
	if (used_quota() + SBS + ds_size >= _quota_limit) {

		PWRN("Quota exceeded: %s", _label);
		PWRN("  memory for slab:               %zd", _ds_slab.consumed());
		PWRN("  used quota:                    %zd", used_quota());
		PWRN("  ds_size:                       %zd", ds_size);
		PWRN("  sizeof(Ram_session_component): %zd", sizeof(Ram_session_component));
		PWRN("  quota_limit:                   %zd", _quota_limit);

		throw Quota_exceeded();
	}

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
	for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
		if (_ram_alloc->alloc_aligned(ds_size, &ds_addr, align_log2)) {
			alloc_succeeded = true;
			break;
		}
	}

	/*
	 * Normally, init's quota equals the size of physical memory and this quota
	 * is distributed among the processes. As we check the quota before
	 * allocating, the allocation should always succeed in theory. However,
	 * fragmentation could cause a failing allocation.
	 */
	if (!alloc_succeeded) {
		PERR("We ran out of physical memory while allocating %zd bytes", ds_size);
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
			Dataspace_component(ds_size, (addr_t)ds_addr, !cached, true, this);
	} catch (Allocator::Out_of_memory) {
		PWRN("Could not allocate metadata");
		throw Out_of_metadata();
	}

	/*
	 * Fill new dataspaces with zeros. For non-cached RAM dataspaces, this
	 * function must also make sure to flush all cache lines related to the
	 * address range used by the dataspace.
	 */
	_clear_ds(ds);

	/* keep track of the used quota for actual payload */
	_payload += ds_size;

	if (verbose)
		PDBG("ds_size=%zd, used_quota=%zd quota_limit=%zd",
		     ds_size, used_quota(), _quota_limit);

	Dataspace_capability result = _ds_ep->manage(ds);

	/* create native shared memory representation of dataspace */
	_export_ram_ds(ds);

	return static_cap_cast<Ram_dataspace>(result);
}


void Ram_session_component::free(Ram_dataspace_capability ds_cap)
{
	_free_ds(dynamic_cast<Dataspace_component *>(_ds_ep->obj_by_cap(ds_cap)));
}


int Ram_session_component::ref_account(Ram_session_capability ram_session_cap)
{
	/* the reference account cannot be defined twice */
	if (_ref_account) return -2;

	Ram_session_component *ref = dynamic_cast<Ram_session_component *>
	                            (_ram_session_ep->obj_by_cap(ram_session_cap));

	/* check if recipient is a valid Ram_session_component */
	if (!ref) return -1;

	/* deny the usage of the ram session as its own ref account */
	/* XXX also check for cycles along the tree of ref accounts */
	if (ref == this) return -3;

	_ref_account = ref;
	_ref_account->_register_ref_account_member(this);
	return 0;
}


int Ram_session_component::transfer_quota(Ram_session_capability ram_session_cap,
                                          size_t amount)
{
	if (verbose)
		PDBG("amount=%zd", amount);
	Ram_session_component *dst = dynamic_cast<Ram_session_component *>
	                            (_ram_session_ep->obj_by_cap(ram_session_cap));

	return _transfer_quota(dst, amount);
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
	_md_alloc(md_alloc, Arg_string::find_arg(args, "ram_quota").long_value(0)),
	_ds_slab(&_md_alloc), _ref_account(0)
{
	Arg_string::find_arg(args, "label").string(_label, sizeof(_label), "");
}


Ram_session_component::~Ram_session_component()
{
	/* destroy all dataspaces */
	for (Dataspace_component *ds; (ds = _ds_slab.first_object()); _free_ds(ds));

	if (_payload != 0)
		PWRN("Remaining payload of %zd in ram session to destroy", _payload);

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
