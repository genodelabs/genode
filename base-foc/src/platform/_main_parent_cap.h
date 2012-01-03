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
		Native_capability cap;
		memcpy(&cap, (void *)&_parent_cap, sizeof(cap));

		/* assemble parent capability from object ID and Fiasco cap */
		return reinterpret_cap_cast<Parent>(
			Native_capability(Fiasco::Fiasco_capability::PARENT_CAP,
		                      cap.local_name()));
	}
}

#endif /* _PLATFORM__MAIN_PARENT_CAP_H_ */
