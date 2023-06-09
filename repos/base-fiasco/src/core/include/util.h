/*
 * \brief   Fiasco utilities
 * \author  Christian Helmuth
 * \date    2006-04-11
 *
 * Is very practical now, but please keep the errors of the l4util pkg in mind.
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
#include <base/stdint.h>
#include <region_map/region_map.h>
#include <util/touch.h>

/* base-internal includes */
#include <base/internal/fiasco_thread_helper.h>
#include <base/internal/page_size.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

/* core includes */
#include <types.h>

namespace Core {

	inline void log_event(const char *s)
	{
		Fiasco::fiasco_tbuf_log(s);
	}

	inline void log_event(const char *s, unsigned v1, unsigned v2, unsigned v3)
	{
		Fiasco::fiasco_tbuf_log_3val(s, v1, v2, v3);
	}

	inline void panic(const char *s)
	{
		using namespace Fiasco;
		outstring(s);
		enter_kdebug("> panic <");
	}

	inline void touch_ro(const void *addr, unsigned size)
	{
		using namespace Fiasco;
		unsigned char const volatile *bptr;
		unsigned char const          *eptr;

		bptr = (unsigned char const volatile *)(((unsigned)addr) & L4_PAGEMASK);
		eptr = (unsigned char const *)(((unsigned)addr + size - 1) & L4_PAGEMASK);
		for ( ; bptr <= eptr; bptr += L4_PAGESIZE)
			touch_read(bptr);
	}

	inline void touch_rw(const void *addr, unsigned size)
	{
		using namespace Fiasco;
		unsigned char volatile *bptr;
		unsigned char const    *eptr;

		bptr = (unsigned char volatile *)(((unsigned)addr) & L4_PAGEMASK);
		eptr = (unsigned char const *)(((unsigned)addr + size - 1) & L4_PAGEMASK);
		for (; bptr <= eptr; bptr += L4_PAGESIZE)
			touch_read_write(bptr);
	}

	inline addr_t trunc_page(addr_t addr)
	{
		using namespace Fiasco;
		return l4_trunc_page(addr);
	}

	inline addr_t round_page(addr_t addr)
	{
		using namespace Fiasco;
		return l4_round_page(addr);
	}

	inline addr_t round_superpage(addr_t addr)
	{
		using namespace Fiasco;
		return l4_round_superpage(addr);
	}

	constexpr size_t get_super_page_size()      { return L4_SUPERPAGESIZE; }
	constexpr size_t get_super_page_size_log2() { return L4_LOG2_SUPERPAGESIZE; }

	inline addr_t map_src_addr(addr_t core_local_addr, addr_t) {
		return core_local_addr; }

	inline Log2 kernel_constrained_map_size(Log2 size) { return size; }

	inline unsigned long convert_native_thread_id_to_badge(Fiasco::l4_threadid_t tid)
	{
		/*
		 * Fiasco has no server-defined badges for page-fault messages.
		 * Therefore, we have to interpret the sender ID as badge.
		 */
		return tid.raw;
	}
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
