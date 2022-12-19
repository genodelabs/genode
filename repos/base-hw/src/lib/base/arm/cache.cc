/*
 * \brief  Implementation of cache operations for ARM
 * \author Stefan Kalkowski
 * \date   2022-12-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/interface.h>

#include <base/internal/cache.h>
#include <cpu/cache.h>

using namespace Genode;


void Genode::cache_coherent(addr_t addr, size_t size)
{
	for_each_page(addr, size, [] (addr_t addr, size_t size) {
		Kernel::cache_coherent_region(addr, size); });
}


void Genode::cache_clean_invalidate_data(addr_t addr, size_t size)
{
	for_each_page(addr, size, [] (addr_t addr, size_t size) {
		Kernel::cache_clean_invalidate_data_region(addr, size); });
}


void Genode::cache_invalidate_data(addr_t addr, size_t size)
{
	for_each_page(addr, size, [] (addr_t addr, size_t size) {
		Kernel::cache_invalidate_data_region(addr, size); });
}

