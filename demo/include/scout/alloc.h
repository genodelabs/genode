/*
 * \brief   Malloc/free wrappers for Genode
 * \date    2008-07-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SCOUT__ALLOC_H_
#define _INCLUDE__SCOUT__ALLOC_H_

#include <base/env.h>

namespace Scout {

	static inline void *malloc(unsigned long size)
	{
		return Genode::env()->heap()->alloc(size);
	}

	static inline void free(void *addr)
	{
		/*
		 * FIXME: We expect the heap to know the size of the
		 *        block and thus, just specify zero as size.
		 */
		Genode::env()->heap()->free(addr, 0); }
}

#endif /* _INCLUDE__SCOUT__ALLOC_H_ */
