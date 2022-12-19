/*
 * \brief  Implementation of the cache maintainance on ARMv8
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
#include <cpu/memory_barrier.h>

using namespace Genode;

void Genode::cache_coherent(addr_t addr, size_t size)
{
	Genode::memory_barrier();

	for_each_page(addr, size, [] (addr_t addr, size_t size) {
		for_each_cache_line(addr, size, [&] (addr_t addr) {
			asm volatile("dc cvau, %0" :: "r" (addr));
			asm volatile("dsb ish");
			asm volatile("ic ivau, %0" :: "r" (addr));
			asm volatile("dsb ish");
			asm volatile("isb");
		});
	});
}


void Genode::cache_clean_invalidate_data(addr_t addr, size_t size)
{
	Genode::memory_barrier();

	for_each_page(addr, size, [&] (addr_t addr, size_t size) {

		for_each_cache_line(addr, size, [&] (addr_t addr) {
			asm volatile("dc civac, %0" :: "r" (addr)); });
	});

	asm volatile("dsb sy");
	asm volatile("isb");
}


void Genode::cache_invalidate_data(addr_t addr, size_t size)
{
	for_each_page(addr, size, [] (addr_t addr, size_t size) {
		Kernel::cache_invalidate_data_region(addr, size); });
}
