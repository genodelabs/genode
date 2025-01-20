/*
 * \brief  Mapping of Genode's capability names to kernel capabilities.
 * \author Stefan Kalkowski
 * \date   2010-12-06
 *
 * This is a Fiasco.OC-specific addition to the process environment.
 */

/*
 * Copyright (C) 2010-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/spin_lock.h>
#include <base/internal/cap_map.h>
#include <base/internal/foc_assert.h>

/* kernel includes */
#include <foc/capability_space.h>

/**
 * We had to change the sematic of l4_task_cap_equal to return whether two
 * capabilities point to the same kernel object instead of whether both
 * capabilities are equal with respect to thier rights. To easily check after
 * a Fiasco.OC upgrade whether the sematic of the kernel patch still matches
 * our expectations below macro can be used.
  */
#ifdef TEST_KERN_CAP_EQUAL
inline bool CHECK_CAP_EQUAL(bool equal, Genode::addr_t cap1,
                                        Genode::addr_t cap2)
{
	unsigned long id1 = Foc::l4_debugger_global_id(cap1),
	              id2 = Foc::l4_debugger_global_id(cap2);
	ASSERT(((id1 == id2) == equal), "CAPS NOT EQUAL!!!");
	return equal;
}
#else
inline bool CHECK_CAP_EQUAL(bool equal, Genode::addr_t,
                                        Genode::addr_t)
{
	return equal;
}
#endif /* TEST_KERN_CAP_EQUAL */


using namespace Genode;


/***********************
 **  Cap_index class  **
 ***********************/

static volatile int _cap_index_spinlock = SPINLOCK_UNLOCKED;


bool Cap_index::higher(Cap_index *n) { return n->_id > _id; }


Cap_index* Cap_index::find_by_id(id_t id)
{
	if (_id == id) return this;

	Cap_index *n = Avl_node<Cap_index>::child(id > _id);
	return n ? n->find_by_id(id) : 0;
}


addr_t Cap_index::kcap() const
{
	return cap_idx_alloc().idx_to_kcap(this);
}


uint8_t Cap_index::inc()
{
	/* con't ref-count index that are controlled by core */
	if (cap_idx_alloc().static_idx(this))
		return 1;

	spinlock_lock(&_cap_index_spinlock);
	uint8_t ret = ++_ref_cnt;
	spinlock_unlock(&_cap_index_spinlock);
	return ret;
}


uint8_t Cap_index::dec()
{
	/* con't ref-count index that are controlled by core */
	if (cap_idx_alloc().static_idx(this))
		return 1;

	spinlock_lock(&_cap_index_spinlock);
	uint8_t ret = --_ref_cnt;
	spinlock_unlock(&_cap_index_spinlock);
	return ret;
}


/****************************
 **  Capability_map class  **
 ****************************/

Cap_index* Capability_map::find(Cap_index::id_t id)
{
	Spin_lock::Guard guard(_lock);

	return _tree.first() ? _tree.first()->find_by_id(id) : 0;
}


Cap_index* Capability_map::insert(Cap_index::id_t id)
{
	Spin_lock::Guard guard(_lock);

	if (_tree.first() && _tree.first()->find_by_id(id))
		return { };

	Cap_index * const i = cap_idx_alloc().alloc_range(1);
	if (i) {
		i->id(id);
		_tree.insert(i);
	}
	return i;
}


Cap_index* Capability_map::insert(Cap_index::id_t id, addr_t kcap)
{
	Spin_lock::Guard guard(_lock);

	/* remove potentially existent entry */
	Cap_index *i = _tree.first() ? _tree.first()->find_by_id(id) : 0;
	if (i)
		_tree.remove(i);

	i = cap_idx_alloc().alloc(kcap);
	if (i) {
		i->id(id);
		_tree.insert(i);
	}
	return i;
}


Cap_index* Capability_map::insert_map(Cap_index::id_t id, addr_t kcap)
{
	using namespace Foc;

	Spin_lock::Guard guard(_lock);

	/* check whether capability id exists */
	Cap_index *i = _tree.first() ? _tree.first()->find_by_id(id) : 0;

	/* if we own the capability already check whether it's the same */
	if (i) {
		l4_msgtag_t tag = l4_task_cap_equal(L4_BASE_TASK_CAP, i->kcap(), kcap);
		if (!CHECK_CAP_EQUAL(l4_msgtag_label(tag), i->kcap(), kcap)) {
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
	i = cap_idx_alloc().alloc_range(1);
	if (!i)
		return 0;

	/* set it's id and insert it into the tree */
	i->id(id);
	_tree.insert(i);

	/* map the given cap to our registry entry */
	auto const msg = l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
	                             l4_obj_fpage(kcap, 0, L4_FPAGE_RWX),
	                             i->kcap() | L4_ITEM_MAP | L4_MAP_ITEM_GRANT);

	if (l4_error(msg)) {
		_tree.remove(i);
		cap_idx_alloc().free(i, 1);
		return 0;
	}

	return i;
}


Capability_map &Genode::cap_map()
{
	static Capability_map map;
	return map;
}


/**********************
 ** Capability_space **
 **********************/

Foc::l4_cap_idx_t Capability_space::alloc_kcap()
{
	return cap_idx_alloc().alloc_range(1)->kcap();
}


void Capability_space::free_kcap(Foc::l4_cap_idx_t kcap)
{
	Cap_index *idx = cap_idx_alloc().kcap_to_idx(kcap);
	cap_idx_alloc().free(idx, 1);
}


Foc::l4_cap_idx_t Capability_space::kcap(Native_capability cap)
{
	if (cap.data() == nullptr)
		raw("Native_capability data is NULL!");

	return cap.data()->kcap();
}
