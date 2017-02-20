/*
 * \brief  Interface to obtain the parent capability for the component
 * \author Norman Feske
 * \date   2015-05-12
 *
 * On seL4, no information is propagated via the '_parent_cap' field of the
 * ELF image.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_
#define _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_

/* Genode includes */
#include <parent/capability.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>


namespace Genode {

	static inline Parent_capability parent_cap()
	{
		Capability_space::Ipc_cap_data const
			ipc_cap_data(Rpc_obj_key(), INITIAL_SEL_PARENT);

		Native_capability cap = Capability_space::import(ipc_cap_data);

		return reinterpret_cap_cast<Parent>(cap);
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_ */
