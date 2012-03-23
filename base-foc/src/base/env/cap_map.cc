/*
 * \brief  Mapping of Genode's capability names to kernel capabilities.
 * \author Stefan Kalkowski
 * \date   2010-12-06
 *
 * This is a Fiasco.OC-specific addition to the process enviroment.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/cap_map.h>
#include <base/native_types.h>


/***********************
 **  Cap_index class  **
 ***********************/

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


Genode::Cap_index* Genode::Capability_map::find(int id)
{
	using namespace Genode;

	Lock_guard<Spin_lock> guard(_lock);

	Cap_index* i = 0;
	if (_tree.first())
		i = _tree.first()->find_by_id(id);
	return i;
}


/****************************
 **  Capability_map class  **
 ****************************/

Genode::Cap_index* Genode::Capability_map::insert(int id)
{
	using namespace Genode;

	Lock_guard<Spin_lock> guard(_lock);

	Cap_index *i = cap_idx_alloc()->alloc(1);
	i->id(id);
	_tree.insert(i);
	return i;
}


Genode::Cap_index* Genode::Capability_map::insert(int id, addr_t kcap)
{
	using namespace Genode;

	Lock_guard<Spin_lock> guard(_lock);

	Cap_index *i = cap_idx_alloc()->alloc(kcap, 1);
	i->id(id);
	_tree.insert(i);
	return i;
}


void Genode::Capability_map::remove(Genode::Cap_index* i)
{
	using namespace Genode;

	Lock_guard<Spin_lock> guard(_lock);

	if (i) {
		if (_tree.first())
			i = _tree.first()->find_by_id(i->id());
		if (i) {
			_tree.remove(i);
			cap_idx_alloc()->free(i,1);
		}
	}
}


Genode::Capability_map* Genode::cap_map()
{
	static Genode::Capability_map map;
	return &map;
}
