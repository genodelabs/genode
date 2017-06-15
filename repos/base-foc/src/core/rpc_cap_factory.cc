/*
 * \brief  Fiasco.OC-specific RPC capability factory
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2011-01-13
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/capability.h>
#include <base/printf.h>
#include <util/misc_math.h>

/* core includes */
#include <rpc_cap_factory.h>
#include <cap_id_alloc.h>
#include <cap_index.h>
#include <platform.h>

/* base-internal includes */
#include <base/internal/cap_alloc.h>
#include <base/internal/foc_assert.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/debugger.h>
#include <l4/sys/factory.h>
#include <l4/sys/task.h>
#include <l4/sys/types.h>
}

using namespace Genode;


/***************************
 **  Cap_index_allocator  **
 ***************************/

Genode::Cap_index_allocator* Genode::cap_idx_alloc()
{
	static Genode::Cap_index_allocator_tpl<Core_cap_index,10*1024> alloc;
	return &alloc;
}


/*******************
 **  Cap_mapping  **
 *******************/

Core_cap_index* Cap_mapping::_get_cap()
{
	int id = platform_specific()->cap_id_alloc()->alloc();
	return static_cast<Core_cap_index*>(cap_map()->insert(id));
}


void Cap_mapping::map(Fiasco::l4_cap_idx_t task)
{
	using namespace Fiasco;

	if (!local.valid() || !Fiasco::Capability::valid(remote))
		return;

	l4_msgtag_t tag = l4_task_map(task, L4_BASE_TASK_CAP,
	                              l4_obj_fpage(local.data()->kcap(), 0, L4_FPAGE_RWX),
	                              ((l4_cap_idx_t)remote) | L4_ITEM_MAP);
	if (l4_msgtag_has_error(tag))
		error("mapping cap failed");
}


Cap_mapping::Cap_mapping(bool alloc, Fiasco::l4_cap_idx_t r)
: local(alloc ? *_get_cap() : *(Native_capability::Data *)nullptr), remote(r) { }


Cap_mapping::Cap_mapping(Native_capability cap, Fiasco::l4_cap_idx_t r)
: local(cap), remote(r) { }


/***********************
 **  Rpc_cap_factory  **
 ***********************/

Native_capability Rpc_cap_factory::alloc(Native_capability ep)
{
	Native_capability cap;

	if (!ep.valid()) {
		warning("Invalid reference capability!");
		return cap;
	}

	try {
		using namespace Fiasco;

		Core_cap_index const * ref = static_cast<Core_cap_index const*>(ep.data());

		ASSERT(ref && ref->pt(), "No valid platform_thread");

		/*
		 * Allocate new id, and ipc-gate and set id as gate-label
		 */
		unsigned long id = platform_specific()->cap_id_alloc()->alloc();
		Core_cap_index* idx = static_cast<Core_cap_index*>(cap_map()->insert(id));

		if (!idx) {
			warning("Out of capability indices!");
			platform_specific()->cap_id_alloc()->free(id);
			return cap;
		}

		l4_msgtag_t tag = l4_factory_create_gate(L4_BASE_FACTORY_CAP,
		                                         idx->kcap(),
		                                         ref->pt()->thread().local.data()->kcap(), id);
		if (l4_msgtag_has_error(tag)) {
			error("l4_factory_create_gate failed!");
			cap_map()->remove(idx);
			platform_specific()->cap_id_alloc()->free(id);
			return cap;
		} else
			/* set debugger-name of ipc-gate to thread's name */
			Fiasco::l4_debugger_set_object_name(idx->kcap(), ref->pt()->name());

		// XXX remove cast
		idx->session((Pd_session_component *)this);
		idx->pt(ref->pt());
		cap = Native_capability(*idx);
	} catch (Cap_id_allocator::Out_of_ids) {
		error("out of capability IDs");
	}

	/*
	 * insert valid capabilities into the session's object pool to
	 * be able to destroy them on session destruction.
	 * For the construction of core's own threads the related cap session
	 * doesn't have an allocator set. But this session gets never destroyed
	 * so this is not an issue.
	 */
	if (cap.valid())
		_pool.insert(new (_entry_slab) Entry(cap));
	return cap;
}


void Rpc_cap_factory::free(Native_capability cap)
{
	using namespace Fiasco;

	if (!cap.valid()) return;

	/* check whether the capability was created by this cap_session */
	if (static_cast<Core_cap_index const *>(cap.data())->session() != (Pd_session_component *)this)
		return;

	Entry * entry;
	_pool.apply(cap, [&] (Entry *e) {
		entry = e;
		if (e) {
			_pool.remove(e);
		} else
			warning("Could not find capability to be deleted");
	});
	if (entry) destroy(_entry_slab, entry);
}


Rpc_cap_factory::~Rpc_cap_factory()
{
	_pool.remove_all([this] (Entry *e) {
		if (!e) return;
		destroy(_entry_slab, e);
	});
}


/*******************************
 **  Capability ID Allocator  **
 *******************************/

Cap_id_allocator::Cap_id_allocator(Allocator* alloc)
: _id_alloc(alloc)
{
	_id_alloc.add_range(CAP_ID_OFFSET, CAP_ID_RANGE);
}


unsigned long Cap_id_allocator::alloc()
{
	Lock::Guard lock_guard(_lock);

	void *id;
	if (_id_alloc.alloc(CAP_ID_OFFSET, &id))
		return (unsigned long) id;
	throw Out_of_ids();
}


void Cap_id_allocator::free(unsigned long id)
{
	Lock::Guard lock_guard(_lock);

	if (id < CAP_ID_RANGE)
		_id_alloc.free((void*)(id & CAP_ID_MASK), CAP_ID_OFFSET);
}


void Genode::Capability_map::remove(Genode::Cap_index* i)
{
	using namespace Genode;
	using namespace Fiasco;

	Lock_guard<Spin_lock> guard(_lock);

	if (i) {
		Core_cap_index* e = static_cast<Core_cap_index*>(_tree.first() ? _tree.first()->find_by_id(i->id()) : 0);
		if (e == i) {
			l4_msgtag_t tag = l4_task_unmap(L4_BASE_TASK_CAP,
			                                l4_obj_fpage(i->kcap(), 0, L4_FPAGE_RWX),
			                                L4_FP_ALL_SPACES | L4_FP_DELETE_OBJ);
			if (l4_msgtag_has_error(tag))
				error("destruction of ipc-gate ", i->kcap(), " failed!");


			platform_specific()->cap_id_alloc()->free(i->id());
			_tree.remove(i);
		}
		cap_idx_alloc()->free(i, 1);
	}
}
