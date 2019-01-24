/*
 * \brief  Capability index allocator for Fiasco.OC non-core processes.
 * \author Stefan Kalkowski
 * \date   2012-02-16
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/cap_alloc.h>

Genode::Cap_index_allocator &Genode::cap_idx_alloc()
{
	static Genode::Cap_index_allocator_tpl<Cap_index,4096> alloc;
	return alloc;
}
