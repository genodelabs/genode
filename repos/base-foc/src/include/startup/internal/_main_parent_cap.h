/*
 * \brief  Obtain parent capability
 * \author Norman Feske
 * \date   2010-01-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__STARTUP__INTERNAL___MAIN_PARENT_CAP_H_
#define _INCLUDE__STARTUP__INTERNAL___MAIN_PARENT_CAP_H_

/* Genode includes */
#include <base/native_types.h>

/* base-internal includes */
#include <base/internal/crt0.h>

namespace Genode {

	/**
	 * Return constructed parent capability
	 */
	Parent_capability parent_cap()
	{
		Native_capability::Raw *raw = (Native_capability::Raw *)&_parent_cap;

		static Cap_index *i = cap_map()->insert(raw->local_name,
		                                        Fiasco::PARENT_CAP);

		/*
		 * Update local name after a parent capability got reloaded via
		 * 'Platform_env::reload_parent_cap()'.
		 */
		if (i->id() != raw->local_name) {
			cap_map()->remove(i);
			i = cap_map()->insert(raw->local_name, Fiasco::PARENT_CAP);
		}

		return reinterpret_cap_cast<Parent>(Native_capability(i));
	}
}

#endif /* _INCLUDE__STARTUP__INTERNAL___MAIN_PARENT_CAP_H_ */
