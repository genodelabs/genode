/*
 * \brief  Core-local RM session
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
	auto lambda = [&] (Dataspace_component *ds) -> Local_addr {
		if (!ds)
			throw Invalid_dataspace();

		if (use_local_addr) {
			PERR("Parameter 'use_local_addr' not supported within core");
			return nullptr;
		}

		if (offset) {
			PERR("Parameter 'offset' not supported within core");
			return nullptr;
		}

		/* allocate range in core's virtual address space */
		return ds->core_local_addr();
	};
	return _ds_ep->apply(ds_cap, lambda);
}
