/*
 * \brief  Obtain parent capability
 * \author Norman Feske
 * \date   2010-01-26
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM__MAIN_PARENT_CAP_H_
#define _PLATFORM__MAIN_PARENT_CAP_H_

#include <base/native_types.h>

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
		if (i->id() != raw->local_name)
			i->id(raw->local_name);

		return reinterpret_cap_cast<Parent>(Native_capability(i));
	}
}

#endif /* _PLATFORM__MAIN_PARENT_CAP_H_ */
