/*
 * \brief  Mapping of Genode's capability names to kernel capabilities.
 * \author Alexander Boettcher
 * \date   2013-08-26
 */

/*
 * Copyright (C) 2013-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/cap_map.h>

#include <base/printf.h>

/* base-nova specific include */
#include <nova/syscalls.h>

using namespace Genode;


Capability_map *Genode::cap_map()
{
	static Genode::Capability_map map;
	return &map;
}


/***********************
 **  Cap_index class  **
 ***********************/


Cap_range *Cap_range::find_by_id(addr_t id)
{
	if (_match(id)) return this;

	Cap_range *obj = this->child(id > _base);
	return obj ? obj->find_by_id(id) : 0;
}


void Cap_range::inc(unsigned id, bool inc_if_one)
{
	bool failure = false;
	{
		Lock::Guard guard(_lock);

		if (inc_if_one && _cap_array[id] != 1)
			return;

		if (_cap_array[id] + 1 == 0)
			failure = true;
		else
			_cap_array[id]++;
	}

	if (failure)
		PERR("cap reference counting error - reference overflow of cap=%lx",
		     _base + id);
}


void Cap_range::dec(unsigned const id_start, bool revoke, unsigned num_log_2)
{
	bool failure = false;
	{
		unsigned const end = min(id_start + (1U << num_log_2), elements());

		Lock::Guard guard(_lock);

		for (unsigned id = id_start; id < end; id++) {
			if (_cap_array[id] == 0) {
				failure = true;
				continue;
			}

			if (revoke && _cap_array[id] == 1)
				Nova::revoke(Nova::Obj_crd(_base + id, 0));

			_cap_array[id]--;
		}
	}

	if (failure)
		PERR("cap reference counting error - one counter of cap range %lx+%x "
		     "has been already zero", _base + id_start, 1 << num_log_2);
}


addr_t Cap_range::alloc(size_t const num_log2)
{
	addr_t const step = 1UL << num_log2;

	{
		Lock::Guard guard(_lock);

		unsigned max  = elements();
		addr_t   last = _last;

		do {

			/* align i to num_log2 */
			unsigned i = ((_base + last + step - 1) & ~(step - 1)) - _base;
			unsigned j;
			for (; i + step < max; i += step) {
				for (j = 0; j < step; j++)
					if (_cap_array[i+j])
						break;
				if (j < step)
					continue;

				for (j = 0; j < step; j++)
					_cap_array[i+j] = 1;

				_last = i;
				return _base + i;
			}

			max  = last;
			last = 0;

		} while (max);
	}

	Cap_range *child = this->child(LEFT);
	if (child) {
		addr_t res = child->alloc(num_log2);
		if (res != ~0UL)
			return res;
	}
	child = this->child(RIGHT);
	if (child) {
		addr_t res = child->alloc(num_log2);
		return res;
	}

	return ~0UL;
}


/****************************
 **  Capability_map class  **
 ****************************/


Cap_index Capability_map::find(Genode::addr_t id) {
	return Cap_index(_tree.first() ? _tree.first()->find_by_id(id) : 0, id); }


addr_t Capability_map::insert(size_t const num_log_2, addr_t const sel)
{
	if (sel == ~0UL)
		return _tree.first() ? _tree.first()->alloc(num_log_2) : ~0UL;

	Cap_range * range = _tree.first() ? _tree.first()->find_by_id(sel) : 0;
	if (!range)
		return ~0UL;

	for (unsigned i = 0; i < 1UL << num_log_2; i++)
		range->inc(sel + i - range->base());

	return sel;
}


void Capability_map::remove(Genode::addr_t const sel, uint8_t num_log_2,
                            bool revoke)
{
	Cap_range * range = _tree.first() ? _tree.first()->find_by_id(sel) : 0;
	if (!range)
		return;

	range->dec(sel - range->base(), revoke, num_log_2);

	Genode::addr_t last_sel   = sel + (1UL << num_log_2);
	Genode::addr_t last_range = range->base() + range->elements();

	while (last_sel > last_range) {
		uint8_t left_log2 = log2(last_sel - last_range);

		remove(last_range, left_log2, revoke);

		last_range += 1UL << left_log2;
	}
}
