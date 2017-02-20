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

#include <linux_syscalls.h>

#include <cpu/cache.h>

void Genode::cache_coherent(Genode::addr_t addr, Genode::size_t size)
{
	lx_syscall(__ARM_NR_cacheflush, addr, addr + size, 0);
}
