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

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <base/printf.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* base-hw includes */
#include <kernel/interface.h>

namespace Genode
{
	struct Native_region
	{
		addr_t base;
		size_t size;
	};

	enum {
		ACTIVITY_TABLE_ON_FAULTS = 0,
	};

	/**
	 * Get the base mask for the minimal supported page-size
	 */
	constexpr addr_t get_page_mask() { return ~(get_page_size() - 1); }


	/**
	 * Round down to the minimal page-size alignment
	 */
	constexpr addr_t trunc_page(addr_t addr) {
		return addr & get_page_mask(); }


	/**
	 * Round up to the minimal page-size alignment
	 */
	constexpr addr_t round_page(addr_t addr)
	{ return trunc_page(addr + get_page_size() - 1); }


	/**
	 * Return an address rounded down to a specific alignment
	 *
	 * \param addr         original address
	 * \param alignm_log2  log2 of the required alignment
	 */
	inline addr_t trunc(addr_t const addr, addr_t const alignm_log2) {
		return (addr >> alignm_log2) << alignm_log2; }


	/**
	 * Return wether a pointer fullfills an alignment
	 *
	 * \param p            pointer
	 * \param alignm_log2  log2 of the required alignment
	 */
	inline bool aligned(void * const p, addr_t const alignm_log2) {
		return (addr_t)p == trunc((addr_t)p, alignm_log2); }


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
	                             Region_map::State::Fault_type const fault_type,
	                             unsigned const faulter_badge);
}


void Genode::print_page_fault(char const * const fault_msg,
                              addr_t const fault_addr,
                              addr_t const fault_ip,
                              Region_map::State::Fault_type const fault_type,
                              unsigned const faulter_badge)
{
	const char read[] = "read from";
	const char write[] = "write to";
	printf("\033[31m%s\033[0m (faulter %x", fault_msg, faulter_badge);
	printf(" with IP %p attempts to", (void *)fault_ip);
	printf(" %s", fault_type == Region_map::State::READ_FAULT ? read : write);
	printf(" address %p)\n", (void *)fault_addr);
	if (ACTIVITY_TABLE_ON_FAULTS) {
		printf("---------- activity table ----------\n");
		Kernel::print_char(0);
		printf("\n");
	}
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
