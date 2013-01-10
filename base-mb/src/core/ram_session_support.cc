/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <ram_session_component.h>


using namespace Genode;

static bool const verbose = false;

void Ram_session_component::_export_ram_ds(Dataspace_component *ds)
{
	if (verbose) PERR("not implemented");
}


void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds)
{
	if (verbose) PERR("not implemented");
}


void Ram_session_component::_clear_ds(Dataspace_component *ds)
{
	/*
	 * We don't have to allocate a core local dataspace to get
	 * virtual access because core is mapped 1-to-1. (except for
	 * its context-area)
	 */
	memset((void *)ds->phys_addr(), 0, ds->size());
}
