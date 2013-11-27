/*
 * \brief   Core-internal utilities
 * \author  Martin Stein
 * \date    2012-01-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <base/printf.h>

namespace Genode
{
	enum {
		ACTIVITY_TABLE_ON_FAULTS = 0,
		MIN_PAGE_SIZE_LOG2 = 12,
	};

	/**
	 * Identification that core threads use to get access to their metadata
	 */
	typedef addr_t Core_thread_id;

	/**
	 * Allows core threads to get their core-thread ID via their stack pointer
	 */
	enum { CORE_STACK_ALIGNM_LOG2 = 15 };

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
	 * Print debug output on page faults
	 *
	 * \param fault_msg      introductory message
	 * \param fault_addr     target address of the fault access
	 * \param fault_ip       instruction pointer of the faulter
	 * \param fault_type     access type of fault
	 * \param faulter_badge  user identification of faulter
	 */
	inline void print_page_fault(char const * const fault_msg,
	                             addr_t const fault_addr,
	                             addr_t const fault_ip,
	                             Rm_session::Fault_type const fault_type,
	                             unsigned const faulter_badge);
}


void Genode::print_page_fault(char const * const fault_msg,
                              addr_t const fault_addr,
                              addr_t const fault_ip,
                              Rm_session::Fault_type const fault_type,
                              unsigned const faulter_badge)
{
	const char read[] = "read from";
	const char write[] = "write to";
	printf("\033[31m%s\033[0m (faulter %x", fault_msg, faulter_badge);
	printf(" with IP %p attempts to", (void *)fault_ip);
	printf(" %s", fault_type == Rm_session::READ_FAULT ? read : write);
	printf(" address %p)\n", (void *)fault_addr);
	if (ACTIVITY_TABLE_ON_FAULTS) {
		printf("---------- activity table ----------\n");
		Kernel::print_char(0);
		printf("\n");
	}
}

#endif /* _UTIL_H_ */

