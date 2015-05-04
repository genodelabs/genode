/*
 * \brief  Export and initialize RAM dataspace
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <ram_session_component.h>
#include <platform.h>
#include <map_local.h>


using namespace Genode;

void Ram_session_component::_export_ram_ds(Dataspace_component *ds)
{
	PDBG("not implemented");
}

void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds)
{
	PDBG("not implemented");
}


void Ram_session_component::_clear_ds (Dataspace_component *ds)
{
	PDBG("not implemented");
}
