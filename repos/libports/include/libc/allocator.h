/*
 * \brief  Genode::Allocator that uses the libc's global heap
 * \author Norman Feske
 * \date   2017-01-31
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LIBC__ALLOCATOR_H_
#define _INCLUDE__LIBC__ALLOCATOR_H_

/* Genode includes */
#include <base/allocator.h>

/* libc includes */
#include <stdlib.h>

namespace Libc { struct Allocator; }


struct Libc::Allocator : Genode::Allocator
{
	typedef Genode::size_t size_t;

	bool alloc(size_t size, void **out_addr) override
	{
		*out_addr = malloc(size);
		return true;
	}

	void free(void *addr, size_t size) override { ::free(addr); }

	bool need_size_for_free() const override { return false; }

	size_t overhead(size_t size) const override { return 0; }
};

#endif /* _INCLUDE__LIBC__ALLOCATOR_H_ */
