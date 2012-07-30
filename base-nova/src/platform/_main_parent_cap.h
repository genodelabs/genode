/*
 * \brief  Obtain parent capability
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2010-01-26
 *
 * The parent portal is, by convention, capability selector 'PT_CAP_PARENT'
 * supplied with the initial portals when the PD is created.
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
		/* assemble parent capability */
		return reinterpret_cap_cast<Parent>(
			Native_capability(Nova::PT_SEL_PARENT));
	}
}

#endif /* _PLATFORM__MAIN_PARENT_CAP_H_ */
