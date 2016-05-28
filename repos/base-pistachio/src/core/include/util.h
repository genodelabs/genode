/*
 * \brief   Pistachio utilities
 * \author  Christian Helmuth
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/printf.h>
#include <rm_session/rm_session.h>
#include <util/touch.h>
#include <base/native_types.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* core-local includes */
#include <kip.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/types.h>
#include <l4/ipc.h>
#include <l4/kdebug.h>
}

namespace Genode {

	inline void log_event(const char *s) { }
	inline void log_event(const char *s, unsigned v1, unsigned v2, unsigned v3) { }

	inline void panic(const char *s)
	{
		using namespace Pistachio;
		PDBG("Panic: %s", s);
		L4_KDB_Enter("> panic <");
	}

	inline void assert(const char *s, bool val)
	{
		using namespace Pistachio;
		if (!val) {
			PERR("Assertion failed: %s", s);
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

	constexpr addr_t get_page_mask()      { return ~(get_page_size() - 1); }

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

	inline void print_page_fault(const char *msg, addr_t pf_addr, addr_t pf_ip,
	                             Region_map::State::Fault_type pf_type,
	                             unsigned long badge)
	{
		Pistachio::L4_ThreadId_t tid;
		tid.raw = badge;
		printf("%s (%s pf_addr=%p pf_ip=%p from %02lx (raw %08lx))\n", msg,
		       pf_type == Region_map::State::WRITE_FAULT ? "WRITE" : "READ",
		       (void *)pf_addr, (void *)pf_ip,
		       Pistachio::L4_GlobalId(tid).global.X.thread_no, tid.raw);
	}

	inline addr_t map_src_addr(addr_t core_local_addr, addr_t phys_addr) {
		return core_local_addr; }

	inline size_t constrain_map_size_log2(size_t size_log2) {
		return size_log2; }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
