/*
 * \brief  Capability helper
 * \author Norman Feske
 * \date   2016-06-27
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA__CAPABILITY_SPACE_H_
#define _INCLUDE__NOVA__CAPABILITY_SPACE_H_

/* Genode includes */
#include <base/capability.h>

/* NOVA includes */
#include <nova/syscalls.h>

namespace Genode { namespace Capability_space {

	static constexpr unsigned long INVALID_INDEX = ~0UL;

	typedef Nova::Crd Ipc_cap_data;

	static inline Nova::Crd crd(Native_capability const &cap)
	{
		/*
		 * We store the 'Nova::Crd' value in place of the 'Data' pointer.
		 */
		addr_t value = (addr_t)cap.data();
		Nova::Crd crd = *(Nova::Crd *)&value;
		return crd;
	}

	static inline Native_capability import(addr_t sel, unsigned rights = 0x1f)
	{
		Nova::Obj_crd const crd = (sel == INVALID_INDEX)
		                        ? Nova::Obj_crd() : Nova::Obj_crd(sel, 0, rights);
		return Native_capability(*(Native_capability::Data *)crd.value());
	}
} }

#endif /* _INCLUDE__NOVA__CAPABILITY_SPACE_H_ */
