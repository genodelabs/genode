/*
 * \brief   Fiasco.OC utilities
 * \author  Christian Helmuth
 * \author  Stefan Kalkowski
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
#include <rm_session/rm_session.h>
#include <util/touch.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

/* core includes */
#include <types.h>

namespace Core {

	inline void panic(const char *s)
	{
		raw(s);
		raw("> panic <");
		while (1) ;
	}

	inline void touch_ro(const void *addr, unsigned size)
	{
		using namespace Foc;
		unsigned char const volatile *bptr;
		unsigned char const *eptr;

		bptr = (unsigned char const volatile *)(((addr_t)addr) & L4_PAGEMASK);
		eptr = (unsigned char const *)(((addr_t)addr + size - 1) & L4_PAGEMASK);
		for ( ; bptr <= eptr; bptr += L4_PAGESIZE)
			touch_read(bptr);
	}

	inline void touch_rw(const void *addr, unsigned size)
	{
		using namespace Foc;

		unsigned char volatile *bptr;
		unsigned char const *eptr;

		bptr = (unsigned char volatile *)(((addr_t)addr) & L4_PAGEMASK);
		eptr = (unsigned char const *)(((addr_t)addr + size - 1) & L4_PAGEMASK);
		for (; bptr <= eptr; bptr += L4_PAGESIZE)
			touch_read_write(bptr);
	}

	inline addr_t trunc_page(addr_t addr) { return Foc::l4_trunc_page(addr); }
	inline addr_t round_page(addr_t addr) { return Foc::l4_round_page(addr); }

	constexpr size_t get_super_page_size()      { return L4_SUPERPAGESIZE;      }
	constexpr size_t get_super_page_size_log2() { return L4_LOG2_SUPERPAGESIZE; }

	inline addr_t map_src_addr(addr_t core_local_addr, addr_t) {
		return core_local_addr; }

	inline Log2 kernel_constrained_map_size(Log2 size) { return size; }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
