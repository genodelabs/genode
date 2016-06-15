/*
 * \brief  Interface to obtain the parent capability for the component
 * \author Norman Feske
 * \date   2015-05-12
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_
#define _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_

/* Genode includes */
#include <parent/capability.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>
#include <base/internal/crt0.h>


namespace Genode {

	static inline Parent_capability parent_cap()
	{
		Native_capability::Raw &raw = *(Native_capability::Raw *)&_parent_cap;

		Okl4::L4_ThreadId_t tid;
		tid.raw = raw.v[0];

		Native_capability cap = Capability_space::import(tid, Rpc_obj_key(raw.v[1]));

		return reinterpret_cap_cast<Parent>(cap);
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_ */
