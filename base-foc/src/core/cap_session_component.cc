/*
 * \brief  Fiasco.oc platform-specific capability allocation
 * \author Stefan Kalkowski
 * \date   2011-01-13
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/capability.h>
#include <base/cap_alloc.h>
#include <util/misc_math.h>

/* core includes */
#include <cap_session_component.h>
#include <cap_id_alloc.h>
#include <cap_index.h>
#include <platform.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/debugger.h>
#include <l4/sys/factory.h>
#include <l4/sys/task.h>
#include <l4/sys/types.h>
}

#include <util/assert.h>

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


void Cap_mapping::map(Native_thread_id task)
{
	using namespace Fiasco;

	if (!local.valid() || !Fiasco::Capability::valid(remote))
		return;

	l4_msgtag_t tag = l4_task_map(task, L4_BASE_TASK_CAP,
	                              l4_obj_fpage(local.dst(), 0, L4_FPAGE_RWX),
	                              ((l4_cap_idx_t)remote) | L4_ITEM_MAP);
	if (l4_msgtag_has_error(tag))
		PERR("mapping cap failed");
}


Cap_mapping::Cap_mapping(bool alloc, Native_thread_id r)
: local(alloc ? _get_cap() : 0), remote(r) { }


Cap_mapping::Cap_mapping(Native_capability cap, Native_thread_id r)
: local(cap), remote(r) { }


/*****************************
 **  Cap_session_component  **
 *****************************/

Native_capability Cap_session_component::alloc(Cap_session_component *session,
                                               Native_capability ep)
{
	Native_capability cap;

	if (!ep.valid()) {
		PWRN("Invalid cap!");
		return cap;
	}

	try {
		using namespace Fiasco;

		Core_cap_index* ref = static_cast<Core_cap_index*>(ep.idx());

		ASSERT(ref && ref->pt(), "No valid platform_thread");

		/*
		 * Allocate new id, and ipc-gate and set id as gate-label
		 */
		unsigned long id = platform_specific()->cap_id_alloc()->alloc();
		Core_cap_index* idx = static_cast<Core_cap_index*>(cap_map()->insert(id));

		if (!idx) {
			PWRN("Out of capabilities!");
			platform_specific()->cap_id_alloc()->free(id);
			return cap;
		}

		l4_msgtag_t tag = l4_factory_create_gate(L4_BASE_FACTORY_CAP,
		                                         idx->kcap(),
		                                         ref->pt()->thread().local.dst(), id);
		if (l4_msgtag_has_error(tag)) {
			PERR("l4_factory_create_gate failed!");
			cap_map()->remove(idx);
			platform_specific()->cap_id_alloc()->free(id);
			return cap;
		} else
			/* set debugger-name of ipc-gate to thread's name */
			Fiasco::l4_debugger_set_object_name(idx->kcap(), ref->pt()->name());

		idx->session(session);
		idx->pt(ref->pt());
		idx->inc();
		cap = Native_capability(idx);
	} catch (Cap_id_allocator::Out_of_ids) {
		PERR("Out of IDs");
	}

	return cap;
}


Native_capability Cap_session_component::alloc(Native_capability ep)
{
	return Cap_session_component::alloc(this, ep);
}


void Cap_session_component::free(Native_capability cap)
{
	using namespace Fiasco;

	if (!cap.valid())
		return;

	Core_cap_index* idx = static_cast<Core_cap_index*>(cap.idx());

	/*
	 * check whether this cap_session has created the capability to delete.
	 */
	if (idx->session() != this)
		return;

	idx->dec();
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
				PERR("destruction of ipc-gate %lx failed!", (unsigned long) i->kcap());


			platform_specific()->cap_id_alloc()->free(i->id());
			_tree.remove(i);
		}
		cap_idx_alloc()->free(i, 1);
	}
}


Genode::Cap_index_allocator* cap_idx_alloc()
{
	static Genode::Cap_index_allocator_tpl<Core_cap_index, 20*1024> _alloc;
	return &_alloc;
}
