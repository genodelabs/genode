/*
 * \brief  Obtain parent capability
 * \author Norman Feske
 * \date   2015-05-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIB__STARTUP___MAIN_PARENT_CAP_H_
#define _LIB__STARTUP___MAIN_PARENT_CAP_H_

/* Genode includes */
#include <util/string.h>

/* base-internal includes */
#include <internal/capability_space_sel4.h>

namespace Genode {

	/**
	 * Return constructed parent capability
	 */
	Parent_capability parent_cap()
	{
		Capability_space::Ipc_cap_data const
			ipc_cap_data(Rpc_obj_key(), INITIAL_SEL_PARENT);

		Native_capability cap = Capability_space::import(ipc_cap_data);

		return reinterpret_cap_cast<Parent>(cap);
	}
}

#endif /* _LIB__STARTUP___MAIN_PARENT_CAP_H_ */
