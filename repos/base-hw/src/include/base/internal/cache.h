/*
 * \brief  Cache maintainance utilities
 * \author Stefan Kalkowski
 * \date   2022-12-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CACHE_H_
#define _INCLUDE__BASE__INTERNAL__CACHE_H_

#include <base/internal/page_size.h>
#include <util/misc_math.h>
#include <util/touch.h>

template <typename FN>
static inline void for_each_page(Genode::addr_t addr,
                                 Genode::size_t size,
                                 FN const     & fn)
{
	using namespace Genode;

	while (size) {
		addr_t next_page = align_addr(addr+1, get_page_size_log2());
		size_t s = min(size, next_page - addr);
		touch_read(reinterpret_cast<unsigned char *>(addr));
		fn(addr, s);
		addr += s;
		size -= s;
	}
}


template <typename FN>
static inline void for_each_cache_line(Genode::addr_t addr,
                                       Genode::size_t size,
                                       FN const     & fn)
{
	using namespace Genode;

	static size_t cache_line_size = Kernel::cache_line_size();

	addr_t start     = addr;
	addr_t const end = addr + size;
	for (; start < end; start += cache_line_size)
		fn(start);
}

#endif /* _INCLUDE__BASE__INTERNAL__CACHE_H_ */
