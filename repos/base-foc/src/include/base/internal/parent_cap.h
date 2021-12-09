/*
 * \brief  Interface to obtain the parent capability for the component
 * \author Norman Feske
 * \date   2013-09-25
 *
 * On Fiasco.OC, we transfer merely the 'local_name' part of the capability
 * via the '_parent_cap' field of the ELF binary.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_
#define _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_

/* Genode includes */
#include <parent/capability.h>
#include <util/string.h>
#include <foc/native_capability.h>

/* base-internal includes */
#include <base/internal/crt0.h>
#include <base/internal/cap_map.h>

namespace Genode {

	static inline Parent_capability parent_cap()
	{
		Cap_index::id_t const local_name = (Cap_index::id_t)_parent_cap;

		static Cap_index *i = cap_map().insert(local_name, Foc::PARENT_CAP);
		/*
		 * Update local name after a parent capability got reloaded via
		 * 'Platform_env::reload_parent_cap()'.
		 */
		if (i->id() != local_name) {
			cap_map().remove(i);
			i = cap_map().insert(local_name, Foc::PARENT_CAP);
		}

		return reinterpret_cap_cast<Parent>(Native_capability(i));
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_ */
