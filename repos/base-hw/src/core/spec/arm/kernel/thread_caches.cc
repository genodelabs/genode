/*
 * \brief   Kernel backend for threads - cache maintainance
 * \author  Stefan Kalkowski
 * \date    2021-06-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw Core includes */
#include <platform_pd.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

using namespace Kernel;


template <typename FN>
static void for_cachelines(addr_t           base,
                           size_t const     size,
                           Kernel::Thread & thread,
                           FN const       & fn)
{
	/**
	 * sanity check that only one small page is affected,
	 * because we only want to lookup one page in the page tables
	 * to limit execution time within the kernel
	 */
	if (Hw::trunc_page(base) != Hw::trunc_page(base+size-1)) {
		Genode::raw(thread, " tried to make cross-page region cache coherent ",
		            (void*)base, " ", size);
		return;
	}

	/**
	 * Lookup whether the page is backed and writeable,
	 * and if so make the memory coherent in between I-, and D-cache
	 */
	addr_t phys = 0;
	if (thread.pd().platform_pd().lookup_rw_translation(base, phys)) {
		fn(base, size);
	} else {
		Genode::raw(thread, " tried to do cache maintainance at ",
					"unallowed address ", (void*)base);
	}
}


void Kernel::Thread::_call_cache_coherent_region()
{
	for_cachelines((addr_t)user_arg_1(), (size_t)user_arg_2(), *this,
	               [] (addr_t addr, size_t size) {
		Genode::Cpu::cache_coherent_region(addr, size); });
}


void Kernel::Thread::_call_cache_clean_invalidate_data_region()
{
	for_cachelines((addr_t)user_arg_1(), (size_t)user_arg_2(), *this,
	               [] (addr_t addr, size_t size) {
		Genode::Cpu::cache_clean_invalidate_data_region(addr, size); });
}


void Kernel::Thread::_call_cache_invalidate_data_region()
{
	for_cachelines((addr_t)user_arg_1(), (size_t)user_arg_2(), *this,
	               [] (addr_t addr, size_t size) {
		Genode::Cpu::cache_invalidate_data_region(addr, size); });
}


void Kernel::Thread::_call_cache_line_size()
{
	size_t const cache_line_size = Genode::Cpu::cache_line_size();
	user_arg_0(cache_line_size);
}
