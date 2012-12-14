/*
 * \brief  Core-local RM session
 * \author MArtin Stein
 * \date   2010-09-09
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <core_rm_session.h>
#include <platform.h>
#include <map_local.h>

using namespace Genode;


Rm_session::Local_addr
Core_rm_session::attach(Dataspace_capability ds_cap, size_t size,
                        off_t offset, bool use_local_addr,
                        Rm_session::Local_addr local_addr,
                        bool executable)
{
	Object_pool<Dataspace_component>::Guard ds(_ds_ep->lookup_and_lock(ds_cap));
	if (!ds)
		throw Invalid_dataspace();

	/* roottask is mapped identically */
	return ds->phys_addr();
}
