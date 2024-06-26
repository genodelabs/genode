/*
 * \brief  Linux protection domain facility
 * \author Norman Feske
 * \date   2006-06-13
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/allocator.h>
#include <parent/parent.h>

/* core includes */
#include <types.h>

namespace Core {

	struct Platform_pd;
	struct Platform_thread;
}


struct Core::Platform_pd
{
	Platform_pd(Allocator &, char const *) { }

	void assign_parent(Capability<Parent>) { }
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
