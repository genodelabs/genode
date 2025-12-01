/*
 * \brief   Pistachio utilities
 * \author  Christian Helmuth
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <util/touch.h>

/* base-internal includes */
#include <base/internal/page_size.h>
#include <base/internal/pistachio.h>

/* core includes */
#include <types.h>
#include <kip.h>
#include <print_l4_thread_id.h>

namespace Core {

	inline void log_event(const char *) { }
	inline void log_event(const char *, unsigned, unsigned, unsigned) { }

	inline void panic(const char *s)
	{
		using namespace Pistachio;
		raw("Panic: ", s);
		L4_KDB_Enter("> panic <");
	}

	inline void assert(const char *s, bool val)
	{
		using namespace Pistachio;
		if (!val) {
			error("Assertion failed: ", s);
			L4_KDB_Enter("Assertion failed.");
		}
	}

	inline void touch_ro(const void *addr, unsigned size)
	{
		using namespace Pistachio;

		uint8_t const volatile *bptr = (uint8_t const volatile *)(addr_t(addr) & PAGE_MASK);
		uint8_t const * const   eptr = (uint8_t const *)((addr_t(addr) + size - 1) & PAGE_MASK);

		for ( ; bptr <= eptr; bptr += PAGE_SIZE)
			touch_read(bptr);
	}

	inline void touch_rw(const void *addr, unsigned size)
	{
		using namespace Pistachio;

		uint8_t volatile     *bptr = (uint8_t volatile *)(addr_t(addr) & PAGE_MASK);
		uint8_t const * const eptr = (uint8_t const *)((addr_t(addr) + size - 1) & PAGE_MASK);

		for(; bptr <= eptr; bptr += PAGE_SIZE)
			touch_read_write(bptr);
	}

	static constexpr uint8_t SUPER_PAGE_SIZE_LOG2 = 22;
	static constexpr size_t  SUPER_PAGE_SIZE = 1 << SUPER_PAGE_SIZE_LOG2;

	inline addr_t trunc_page(addr_t addr) { return addr & PAGE_MASK; }

	inline addr_t round_page(addr_t addr)
	{
		return trunc_page(addr + PAGE_SIZE - 1);
	}

	inline addr_t map_src_addr(addr_t core_local_addr, addr_t) {
		return core_local_addr; }

	inline Log2 kernel_constrained_map_size(Log2 size) { return size; }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
