/*
 * \brief  Mapping of Genode's capability names to kernel capabilities.
 * \author Stefan Kalkowski
 * \date   2010-12-06
 *
 * This is a Fiasco.OC-specific addition to the process enviroment.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cap_map.h>
#include <base/native_types.h>

#include <util/assert.h>

/* base-internal includes */
#include <base/internal/spin_lock.h>

/* kernel includes */
namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/task.h>
}


/***********************
 **  Cap_index class  **
 ***********************/

static volatile int _cap_index_spinlock = SPINLOCK_UNLOCKED;


bool Genode::Cap_index::higher(Genode::Cap_index *n) { return n->_id > _id; }


Genode::Cap_index* Genode::Cap_index::find_by_id(Genode::uint16_t id)
{
	using namespace Genode;

	if (_id == id) return this;

	Cap_index *n = Avl_node<Cap_index>::child(id > _id);
	return n ? n->find_by_id(id) : 0;
}


Genode::addr_t Genode::Cap_index::kcap() {
	return cap_idx_alloc()->idx_to_kcap(this); }


Genode::uint8_t Genode::Cap_index::inc()
{
	/* con't ref-count index that are controlled by core */
	if (cap_idx_alloc()->static_idx(this))
		return 1;

	spinlock_lock(&_cap_index_spinlock);
	Genode::uint8_t ret = ++_ref_cnt;
	spinlock_unlock(&_cap_index_spinlock);
	return ret;
}


Genode::uint8_t Genode::Cap_index::dec()
{
	/* con't ref-count index that are controlled by core */
	if (cap_idx_alloc()->static_idx(this))
		return 1;

	spinlock_lock(&_cap_index_spinlock);
	Genode::uint8_t ret = --_ref_cnt;
	spinlock_unlock(&_cap_index_spinlock);
	return ret;
}


/****************************
 **  Capability_map class  **
 ****************************/

Genode::Cap_index* Genode::Capability_map::find(int id)
{
	Genode::Lock_guard<Spin_lock> guard(_lock);

	return _tree.first() ? _tree.first()->find_by_id(id) : 0;
}


Genode::Cap_index* Genode::Capability_map::insert(int id)
{
	using namespace Genode;

	Lock_guard<Spin_lock> guard(_lock);

	ASSERT(!_tree.first() || !_tree.first()->find_by_id(id),
	       "Double insertion in cap_map()!");

	Cap_index *i = cap_idx_alloc()->alloc_range(1);
	if (i) {
		i->id(id);
		_tree.insert(i);
	}
	return i;
}


Genode::Cap_index* Genode::Capability_map::insert(int id, addr_t kcap)
{
	using namespace Genode;

	Lock_guard<Spin_lock> guard(_lock);

	/* remove potentially existent entry */
	Cap_index *i = _tree.first() ? _tree.first()->find_by_id(id) : 0;
	if (i)
		_tree.remove(i);

	i = cap_idx_alloc()->alloc(kcap);
	if (i) {
		i->id(id);
		_tree.insert(i);
	}
	return i;
}


Genode::Cap_index* Genode::Capability_map::insert_map(int id, addr_t kcap)
{
	using namespace Genode;
	using namespace Fiasco;

	Lock_guard<Spin_lock> guard(_lock);

	/* check whether capability id exists */
	Cap_index *i = _tree.first() ? _tree.first()->find_by_id(id) : 0;

	/* if we own the capability already check whether it's the same */
	if (i) {
		l4_msgtag_t tag = l4_task_cap_equal(L4_BASE_TASK_CAP, i->kcap(), kcap);
		if (!l4_msgtag_label(tag)) {
			/*
			 * they aren't equal, possibly an already revoked cap,
			 * otherwise it's a fake capability and we return an invalid one
			 */
			tag = l4_task_cap_valid(L4_BASE_TASK_CAP, i->kcap());
			if (l4_msgtag_label(tag))
				return 0;
			else
				/* it's invalid so remove it from the tree */
				_tree.remove(i);
		} else
			/* they are equal so just return the one in the map */
			return i;
	}

	/* the capability doesn't exists in the map so allocate a new one */
	i = cap_idx_alloc()->alloc_range(1);
	if (!i)
		return 0;

	/* set it's id and insert it into the tree */
	i->id(id);
	_tree.insert(i);

	/* map the given cap to our registry entry */
	l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
				l4_obj_fpage(kcap, 0, L4_FPAGE_RWX),
				i->kcap() | L4_ITEM_MAP | L4_MAP_ITEM_GRANT);
	return i;
}


Genode::Capability_map* Genode::cap_map()
{
	static Genode::Capability_map map;
	return &map;
}
