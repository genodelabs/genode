/*
 * \brief   OKL4 utilities
 * \author  Norman Feske
 * \date    2009-03-31
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
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
#include <base/internal/okl4.h>

/* core includes */
#include <types.h>

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


namespace Core {

	inline void log_event(const char *) { }
	inline void log_event(const char *, unsigned, unsigned, unsigned) { }

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

	static constexpr size_t SUPER_PAGE_SIZE_LOG2 = 22;

	inline void touch_ro(const void *addr, unsigned size)
	{
		using namespace Okl4;

		uint8_t const volatile *bptr = (uint8_t const *)(addr_t(addr) & PAGE_MASK);
		uint8_t const * const   eptr = (uint8_t const *)((addr_t(addr) + size - 1) & PAGE_MASK);

		for ( ; bptr <= eptr; bptr += PAGE_SIZE)
			touch_read(bptr);
	}

	inline void touch_rw(const void *addr, unsigned size)
	{
		using namespace Okl4;

		uint8_t volatile *bptr = (uint8_t volatile *)(addr_t(addr) & PAGE_MASK);
		uint8_t const    *eptr = (uint8_t const *)((addr_t(addr) + size - 1) & PAGE_MASK);

		for(; bptr <= eptr; bptr += PAGE_SIZE)
			touch_read_write(bptr);
	}

	inline addr_t trunc_page(addr_t page) { return page & PAGE_MASK; }

	inline addr_t round_page(addr_t page) { return trunc_page(page + PAGE_SIZE - 1); }

	inline addr_t map_src_addr(addr_t, addr_t phys) { return phys; }

	inline Log2 kernel_constrained_map_size(Log2 size) { return size; }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
