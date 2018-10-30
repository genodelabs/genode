/*
 * \brief  Capability helper
 * \author Norman Feske
 * \date   2016-06-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_H_
#define _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_H_

/* Genode includes */
#include <base/capability.h>

/* base-hw includes */
#include <kernel/interface.h>

namespace Genode { namespace Capability_space {

	/**
	 * Return kernel capability selector of Genode capability
	 */
	static inline Kernel::capid_t capid(Native_capability const &cap)
	{
		addr_t const index = (addr_t)cap.data();
		return index;
	}

	static inline Native_capability import(Kernel::capid_t capid)
	{
		return Native_capability((Native_capability::Data *)(addr_t)capid);
	}
} }

#endif /* _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_H_ */
