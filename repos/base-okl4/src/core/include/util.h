/*
 * \brief   OKL4 utilities
 * \author  Norman Feske
 * \date    2009-03-31
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <base/stdint.h>
#include <base/log.h>
#include <util/touch.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/types.h>
#include <l4/kdebug.h>
} }

/*
 * The binding for 'L4_KDB_Enter' on ARM takes a 'char *' as argument, which
 * prevents us from passing a plain const "string". However, on x86, the
 * binding is a preprocessor macro accepting only a "string" as argument.
 * Unless the OKL4 bindings get fixed, we have to handle both cases separately.
 */
#ifdef __L4__X86__KDEBUG_H__
#define ENTER_KDB(msg) L4_KDB_Enter(msg);
#else
#define ENTER_KDB(msg) L4_KDB_Enter((char *)msg);
#endif


namespace Genode {

	inline void log_event(const char *s) { }
	inline void log_event(const char *s, unsigned v1, unsigned v2, unsigned v3) { }

	inline void panic(const char *s)
	{
		using namespace Okl4;
		error("Panic: ", s);
		ENTER_KDB("> panic <");
	}

	inline void assert(const char *s, bool val)
	{
		using namespace Okl4;
		if (!val) {
			error("assertion failed: ", s);
			ENTER_KDB("Assertion failed");
		}
	}

	constexpr addr_t get_page_mask()      { return ~(get_page_size() - 1); }

	inline size_t get_super_page_size_log2()
	{
		enum { SUPER_PAGE_SIZE_LOG2 = 22 };
		if (get_page_mask() & (1 << SUPER_PAGE_SIZE_LOG2))
			return SUPER_PAGE_SIZE_LOG2;

		/* if super pages are not supported, return default page size */
		return get_page_size();
	}

	inline void touch_ro(const void *addr, unsigned size)
	{
		using namespace Okl4;
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
		using namespace Okl4;
		unsigned char volatile *bptr;
		unsigned char const    *eptr;
		L4_Word_t mask = get_page_mask();
		L4_Word_t psize = get_page_size();

		bptr = (unsigned char volatile *)(((unsigned)addr) & mask);
		eptr = (unsigned char const *)(((unsigned)addr + size - 1) & mask);
		for(; bptr <= eptr; bptr += psize)
			touch_read_write(bptr);
	}

	inline addr_t trunc_page(addr_t page)
	{
		return page & get_page_mask();
	}

	inline addr_t round_page(addr_t page)
	{
		return trunc_page(page + get_page_size() - 1);
	}

	inline addr_t map_src_addr(addr_t core_local, addr_t phys) { return phys; }

	inline size_t constrain_map_size_log2(size_t size_log2) { return size_log2; }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
