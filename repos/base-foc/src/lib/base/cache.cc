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

#include <cpu/cache.h>
#include <foc/syscall.h>

using namespace Genode;


void Genode::cache_coherent(addr_t addr, size_t size)
{
	Foc::l4_cache_coherent(addr, addr + size);
}


void Genode::cache_clean_invalidate_data(Genode::addr_t addr, Genode::size_t size)
{
	Foc::l4_cache_flush_data(addr, addr + size);
}


void Genode::cache_invalidate_data(Genode::addr_t addr, Genode::size_t size)
{
	Foc::l4_cache_inv_data(addr, addr + size);
}
