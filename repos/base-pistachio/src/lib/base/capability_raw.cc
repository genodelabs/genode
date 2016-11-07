/*
 * \brief  Capability
 * \author Norman Feske
 * \date   2016-06-16
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

using namespace Genode;


Native_capability::Raw Native_capability::raw() const
{
	Capability_space::Ipc_cap_data const cap_data =
		Capability_space::ipc_cap_data(*this);

	return { { cap_data.dst.raw, cap_data.rpc_obj_key.value(), 0, 0 } };
}
