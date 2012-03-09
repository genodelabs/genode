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
#include <util/misc_math.h>

/* core includes */
#include <cap_session_component.h>
#include <platform.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/debugger.h>
#include <l4/sys/factory.h>
#include <l4/sys/types.h>
}

using namespace Genode;


/*****************************
 **  Cap_session_component  **
 *****************************/

Native_capability Cap_session_component::alloc(Cap_session_component *session,
                                               Native_capability ep)
{
	Native_capability cap;

	if (!ep.valid()) {
		PWRN("Invalid cap!");
		return Native_capability();
	}

	/*
	 * maybe someone tries to fool us, proof whether cap exists in cap tree.
	 *
	 * Actually we should proof whether both capability selectors
	 * point to the same object, but this isn't possible in fiasco.oc.
	 */
	Capability_node *n = Capability_tree::tree()->find_by_badge(ep.local_name());
	if (!n) {
		PWRN("Unknown capability!");
		return cap;
	}


	try {
		using namespace Fiasco;

		/*
		 * Allocate new badge, and ipc-gate and set badge as gate-label
		 */
		unsigned long badge = Badge_allocator::allocator()->alloc();
		Native_thread gate  = cap_alloc()->alloc_id(badge);
		l4_msgtag_t   tag   = l4_factory_create_gate(L4_BASE_FACTORY_CAP,
		                                             gate,
		                                             n->pt()->native_thread(),
		                                             badge);
		if (l4_msgtag_has_error(tag)) {
			PERR("l4_factory_create_gate failed!");
			cap_alloc()->free(gate);
			Badge_allocator::allocator()->free(badge);
			return cap;
		} else
			/* set debugger-name of ipc-gate to thread's name */
			Fiasco::l4_debugger_set_object_name(gate, n->pt()->name());

		/*
		 * Create new node in capability tree.
		 *
		 * TODO: don't use core_mem_alloc, but a session-specific allocator
		 */
		Capability_node *new_node = new (platform()->core_mem_alloc())
		                            Capability_node(badge, session, n->pt(), gate);
		Capability_tree::tree()->insert(new_node);
		cap = Native_capability(gate, badge);

	} catch (Badge_allocator::Out_of_badges) {}

	return cap;
}


Native_capability Cap_session_component::alloc(Native_capability ep)
{
	return Cap_session_component::alloc(this, ep);
}


void Cap_session_component::free(Native_capability cap)
{
	using namespace Fiasco;

	Capability_node *n = Capability_tree::tree()->find_by_badge(cap.local_name());
	if (!n)
		return;

	/*
	 * check whether this cap_session has created the capability to delete.
	 *
	 * Actually we should proof whether both capability selectors
	 * point to the same object, but this isn't possible in fiasco.oc.
	 */
	if (n->cap_session() != this)
		return;

	Capability_tree::tree()->remove(n);
	l4_msgtag_t tag = l4_task_unmap(L4_BASE_TASK_CAP,
	                                l4_obj_fpage(cap.dst(), 0, L4_FPAGE_RWX),
	                                L4_FP_ALL_SPACES | L4_FP_DELETE_OBJ);
	if (l4_msgtag_has_error(tag))
		PERR("destruction of ipc-gate %lx failed!", (unsigned long) cap.dst());

	/* free badge _after_ invalidating all caps */
	Badge_allocator::allocator()->free(n->badge());

	/* free explicilty allocated cap-selector */
	cap_alloc()->free(n->gate());

	destroy(platform_specific()->core_mem_alloc(), n);
}


/***********************
 **  Badge Allocator  **
 ***********************/

Badge_allocator::Badge_allocator()
: _id_alloc(platform_specific()->core_mem_alloc())
{
	_id_alloc.add_range(BADGE_OFFSET, BADGE_RANGE);
}


unsigned long Badge_allocator::alloc()
{
	Lock::Guard lock_guard(_lock);

	void *badge;
	if (_id_alloc.alloc(BADGE_OFFSET, &badge))
		return (unsigned long) badge;
	throw Out_of_badges();
}


void Badge_allocator::free(unsigned long badge)
{
	Lock::Guard lock_guard(_lock);

	if (badge < BADGE_RANGE)
		_id_alloc.free((void*)(badge & BADGE_MASK), BADGE_OFFSET);
}


Badge_allocator* Badge_allocator::allocator()
{
	static Badge_allocator alloc;
	return &alloc;
}


/***********************
 **  Capability_node  **
 ***********************/

Capability_node::Capability_node(unsigned long          badge,
                                 Cap_session_component *cap_session,
                                 Platform_thread       *pt,
                                 Native_thread          gate)
: _badge(badge), _cap_session(cap_session), _pt(pt), _gate(gate) {}


bool Capability_node::higher(Capability_node *n)
{
	return n->_badge > _badge;
}


Capability_node* Capability_node::find_by_badge(unsigned long badge)
{
	if (_badge == badge) return this;

	Capability_node *n = Avl_node<Capability_node>::child(badge > _badge);
	return n ? n->find_by_badge(badge) : 0;
}


/***********************
 **  Capability_tree  **
 ***********************/

void Capability_tree::insert(Avl_node<Capability_node> *node)
{
	Lock::Guard lock_guard(_lock);

	Avl_tree<Capability_node>::insert(node);
}


void Capability_tree::remove(Avl_node<Capability_node> *node)
{
	Lock::Guard lock_guard(_lock);

	Avl_tree<Capability_node>::remove(node);
}


Capability_node* Capability_tree::find_by_badge(unsigned long badge)
{
	Lock::Guard lock_guard(_lock);

	return first()->find_by_badge(badge);
}


Capability_tree* Capability_tree::tree()
{
	static Capability_tree _tree;
	return &_tree;
}


Genode::Capability_allocator* Genode::cap_alloc()
{
	static Genode::Capability_allocator_tpl<20*1024> _alloc;
	return &_alloc;
}
