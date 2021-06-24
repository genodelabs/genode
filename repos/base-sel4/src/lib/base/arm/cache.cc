/*
 * \brief  Implementation of the cache operations
 * \author Stefan Kalkowski
 * \date   2021-06-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <cpu/cache.h>

void Genode::cache_coherent(Genode::addr_t, Genode::size_t)
{
	error(__func__, " not implemented for this kernel!");
}


void Genode::cache_clean_invalidate_data(Genode::addr_t, Genode::size_t)
{
	error(__func__, " not implemented for this kernel!");
}


void Genode::cache_invalidate_data(Genode::addr_t, Genode::size_t)
{
	error(__func__, " not implemented for this kernel!");
}
