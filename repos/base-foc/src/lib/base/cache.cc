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

namespace Fiasco {
#include <l4/sys/cache.h>
}

#include <cpu/cache.h>

void Genode::cache_coherent(Genode::addr_t addr, Genode::size_t size)
{
	Fiasco::l4_cache_coherent(addr, addr + size);
}
