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

	using Genode::get_page_size;

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
		unsigned char const volatile *bptr;
		unsigned char const          *eptr;
		L4_Word_t mask = get_page_mask();
		L4_Word_t psize = get_page_size();

		bptr = (unsigned char const volatile *)(((unsigned)addr) & mask);
		eptr = (unsigned char const *)(((unsigned)addr + size - 1) & mask);
		for ( ; bptr <= eptr; bptr += psize)
			touch_read(bptr);
	}

	inline void touch_rw(const void *addr, unsigned size)
	{
		using namespace Pistachio;
		unsigned char volatile *bptr;
		unsigned char const    *eptr;
		L4_Word_t mask = get_page_mask();
		L4_Word_t psize = get_page_size();

		bptr = (unsigned char volatile *)(((unsigned)addr) & mask);
		eptr = (unsigned char const *)(((unsigned)addr + size - 1) & mask);
		for(; bptr <= eptr; bptr += psize)
			touch_read_write(bptr);
	}

	constexpr addr_t get_page_mask() { return ~(get_page_size() - 1); }

	inline size_t get_super_page_size_log2()
	{
		enum { SUPER_PAGE_SIZE_LOG2 = 22 };
		if (get_page_mask() & (1 << SUPER_PAGE_SIZE_LOG2))
			return SUPER_PAGE_SIZE_LOG2;

		/* if super pages are not supported, return default page size */
		return get_page_size();
	}

	inline size_t get_super_page_size() { return 1 << get_super_page_size_log2(); }

	inline addr_t trunc_page(addr_t addr)
	{
		return addr & get_page_mask();
	}

	inline addr_t round_page(addr_t addr)
	{
		return trunc_page(addr + get_page_size() - 1);
	}

	inline addr_t map_src_addr(addr_t core_local_addr, addr_t) {
		return core_local_addr; }

	inline Log2 kernel_constrained_map_size(Log2 size) { return size; }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
