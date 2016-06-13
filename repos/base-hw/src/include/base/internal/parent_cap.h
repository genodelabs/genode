/*
 * \brief  Interface to obtain the parent capability for the component
 * \author Stefan Kalkowski
 * \date   2015-04-27
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

namespace Hw { extern Genode::Untyped_capability _parent_cap; }

namespace Genode {

	static inline Parent_capability parent_cap()
	{
		return reinterpret_cap_cast<Parent>(Hw::_parent_cap);
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_ */
