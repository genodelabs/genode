/*
 * \brief  Default implementation of core-local region map
 * \author Norman Feske
 * \date   2009-04-02
 *
 * This implementation assumes that dataspaces are identity-mapped within
 * core. This is the case for traditional L4 kernels.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <core_local_rm.h>
#include <dataspace_component.h>

using namespace Core;


Core_local_rm::Result
Core_local_rm::attach(Dataspace_capability ds_cap, Attach_attr const &)
{
	return _ep.apply(ds_cap, [&] (Dataspace_component *ds) -> Result {
		if (!ds)
			return Error::INVALID_DATASPACE;

		return { *this, { (void *)ds->phys_addr(), ds->size() } };
	});
}


void Core_local_rm::_free(Attachment &) { }
