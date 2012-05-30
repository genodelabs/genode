/*
 * \brief   Core-internal utilities
 * \author  Martin Stein
 * \date    2012-01-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <base/printf.h>

namespace Genode
{
	enum { MIN_PAGE_SIZE_LOG2 = 12 };


	/**
	 * Get the the minimal supported page-size log 2
	 */
	inline size_t get_page_size_log2()    { return MIN_PAGE_SIZE_LOG2; }


	/**
	 * Get the the minimal supported page-size
	 */
	inline size_t get_page_size() { return 1 << get_page_size_log2(); }


	/**
	 * Get the base mask for the minimal supported page-size
	 */
	inline addr_t get_page_mask() { return ~(get_page_size() - 1); }


	/**
	 * Round down to the minimal page-size alignment
	 */
	inline addr_t trunc_page(addr_t addr) { return addr & get_page_mask(); }


	/**
	 * Round up to the minimal page-size alignment
	 */
	inline addr_t round_page(addr_t addr)
	{ return trunc_page(addr + get_page_size() - 1); }


	/**
	 * Round down to a specific alignment
	 */
	inline addr_t trunc(addr_t const addr, unsigned const alignm_log2)
	{ return addr & ~((1 << alignm_log2) - 1); }


	/**
	 * Round up to a specific alignment
	 */
	inline addr_t round(addr_t const addr, unsigned const alignm_log2)
	{ return trunc(addr + (1<<alignm_log2) - 1, alignm_log2); }


	/**
	 * Select source used for map operations
	 */
	inline addr_t map_src_addr(addr_t core_local, addr_t phys) { return phys; }


	/**
	 * Return highest supported flexpage size for the given mapping size
	 *
	 * This function is called by the page-fault handler to determine the
	 * mapping granularity to be used for a page-fault answer. If a kernel
	 * supports flexible page sizes, this function can just return the
	 * argument. If a kernel only supports a certain set of map sizes such
	 * as 4K and 4M, this function should select one of those smaller or
	 * equal to the argument.
	 */
	inline size_t constrain_map_size_log2(size_t size_log2)
	{
		if (size_log2<20) return 12;
		return 20;
	}


	/**
	 * Debug output on page faults
	 */
	inline void print_page_fault(const char *msg, addr_t pf_addr, addr_t pf_ip,
	                             Rm_session::Fault_type pf_type,
	                             unsigned long faulter_badge)
	{
		printf("%s (%s pf_addr=%p pf_ip=%p from %02lx)", msg,
		       pf_type == Rm_session::WRITE_FAULT ? "WRITE" : "READ",
		       (void *)pf_addr, (void *)pf_ip,
		       faulter_badge);
	}
}

#endif /* _CORE__INCLUDE__UTIL_H_ */

