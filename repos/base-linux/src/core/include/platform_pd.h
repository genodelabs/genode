/*
 * \brief  Linux protection domain facility
 * \author Norman Feske
 * \date   2006-06-13
 *
 * Pretty dumb.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

#include <base/allocator.h>

namespace Genode {
	struct Platform_pd;
	struct Platform_thread;
}

struct Genode::Platform_pd
{
	Platform_pd(Allocator *, char const *) { }

	bool bind_thread(Platform_thread *) { return true; }
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
