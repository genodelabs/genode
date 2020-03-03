/*
 * \brief  Implementation of the cache operations
 * \author Christian Prochaska
 * \date   2014-05-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/interface.h>

#include <base/internal/page_size.h>
#include <cpu/cache.h>
#include <util/misc_math.h>

void Genode::cache_coherent(Genode::addr_t addr, Genode::size_t size)
{
	using namespace Genode;

	/**
	 * The kernel accepts the 'cache_coherent_region' call for one designated
	 * page only. Otherwise, it just ignores the call to limit the time being
	 * uninteruppptible in the kernel. Therefor, we have to loop if more than
	 * one page is affected by the given region.
	 */
	while (size) {
		addr_t next_page = align_addr(addr+1, get_page_size_log2());
		size_t s = min(size, next_page - addr);
		Kernel::cache_coherent_region(addr, s);
		addr += s;
		size -= s;
	}
}
