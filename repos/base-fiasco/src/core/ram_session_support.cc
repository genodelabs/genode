/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Norman Feske
 * \date   2006-07-03
 *
 * On L4, each dataspace _is_ a shared memory object.
 * Therefore, these functions are empty.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "ram_session_component.h"

using namespace Genode;

void Ram_session_component::_export_ram_ds(Dataspace_component *ds) { }
void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }

void Ram_session_component::_clear_ds(Dataspace_component *ds)
{
	memset((void *)ds->phys_addr(), 0, ds->size());
}
