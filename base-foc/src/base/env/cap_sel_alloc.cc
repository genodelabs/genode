/*
 * \brief  Capability-selector allocator for non-core tasks.
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

/* Genode includes */
#include <base/cap_sel_alloc.h>


Genode::Capability_allocator* Genode::cap_alloc()
{
	static Genode::Capability_allocator_tpl<8192> _alloc;
	return &_alloc;
}
