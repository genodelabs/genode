/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Norman Feske
 * \date   2006-07-03
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core-local includes */
#include <ram_session_component.h>
#include <map_local.h>

namespace Fiasco {
#include <l4/sys/cache.h>
}

using namespace Genode;

void Ram_session_component::_export_ram_ds(Dataspace_component *ds) { }
void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }


void Ram_session_component::_clear_ds(Dataspace_component *ds)
{
	memset((void *)ds->phys_addr(), 0, ds->size());

	if (ds->write_combined())
		Fiasco::l4_cache_clean_data((Genode::addr_t)ds->phys_addr(),
		                            (Genode::addr_t)ds->phys_addr() + ds->size());
}

