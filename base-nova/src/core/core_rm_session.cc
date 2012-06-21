/*
 * \brief  Core-local RM session
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <core_rm_session.h>
#include <platform.h>
#include <util.h>
#include <nova_util.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;


Rm_session::Local_addr
Core_rm_session::attach(Dataspace_capability ds_cap, size_t size,
                        off_t offset, bool use_local_addr,
                        Rm_session::Local_addr local_addr,
                        bool executable)
{
	Dataspace_component *ds = static_cast<Dataspace_component *>(_ds_ep->obj_by_cap(ds_cap));
	if (!ds)
		throw Invalid_dataspace();

	if (size == 0)
		size = ds->size();

	if (use_local_addr) {
		PERR("Parameter 'use_local_addr' not supported within core");
		return 0UL;
	}

	if (offset) {
		PERR("Parameter 'offset' not supported within core");
		return 0UL;
	}

	/* allocate range in core's virtual address space */
	return ds->core_local_addr();
}
