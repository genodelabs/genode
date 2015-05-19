/*
 * \brief  Obtain parent capability
 * \author Stefan Kalkowski
 * \date   2015-04-27
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM__MAIN_PARENT_CAP_H_
#define _PLATFORM__MAIN_PARENT_CAP_H_

/* Genode includes */
#include <parent/capability.h>

namespace Hw {
	extern Genode::Untyped_capability _parent_cap;
}

namespace Genode {

	/**
	 * Return constructed parent capability
	 */
	Parent_capability parent_cap()
	{
		/* assemble parent capability */
		return reinterpret_cap_cast<Parent>(Hw::_parent_cap);
	}
}

#endif /* _PLATFORM__MAIN_PARENT_CAP_H_ */
