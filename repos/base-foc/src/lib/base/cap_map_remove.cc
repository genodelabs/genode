/*
 * \brief  Mapping of Genode's capability names to kernel capabilities.
 * \author Stefan Kalkowski
 * \date   2010-12-06
 *
 * This is a Fiasco.OC-specific addition to the process enviroment.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/cap_map.h>

using namespace Genode;


void Capability_map::remove(Genode::Cap_index* i)
{
	Spin_lock::Guard guard(_lock);

	if (i) {
		Cap_index *e = _tree.first() ? _tree.first()->find_by_id(i->id()) : 0;
		if (e == i)
			_tree.remove(i);
		cap_idx_alloc().free(i, 1);
	}
}
