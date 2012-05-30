/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <ram_session_component.h>

using namespace Genode;


void Ram_session_component::_export_ram_ds(Dataspace_component *ds) { }


void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }


void Ram_session_component::_clear_ds (Dataspace_component * ds)
{ memset((void *)ds->phys_addr(), 0, ds->size()); }

