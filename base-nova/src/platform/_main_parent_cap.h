/*
 * \brief  Obtain parent capability
 * \author Norman Feske
 * \date   2010-01-26
 *
 * On NOVA, the parent capability consists of two parts, a local portal
 * capability selector (as invokation address) and a global unique object ID.
 * The parent portal is, by convention, capability selector 'PT_CAP_PARENT'
 * supplied with the initial portals when the PD is created. The object ID is
 * provided at the begin of the data segment of the loaded ELF image.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM__MAIN_PARENT_CAP_H_
#define _PLATFORM__MAIN_PARENT_CAP_H_

/* Genode includes */
#include <util/string.h>

/* NOVA includes */
#include <nova/syscalls.h>

namespace Genode {

	/**
	 * Return constructed parent capability
	 */
	Parent_capability parent_cap()
	{
		/* read capability from start of data section, containing object ID */
		Native_capability cap;
		memcpy(&cap, (void *)&_parent_cap, sizeof(cap));

		/* assemble parent capability from object ID and portal */
		return reinterpret_cap_cast<Parent>(Native_capability(Nova::PT_SEL_PARENT,
		                                                      cap.unique_id()));
	}
}

#endif /* _PLATFORM__MAIN_PARENT_CAP_H_ */
