/*
 * \brief   Core-internal utilities
 * \author  Norman Feske
 * \date    2009-10-02
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
#include <base/printf.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* core includes */
#include <core_cspace.h>


namespace Genode {

	constexpr addr_t get_page_mask()      { return ~(get_page_size() - 1); }
	inline addr_t trunc_page(addr_t addr) { return addr & get_page_mask(); }
	inline addr_t round_page(addr_t addr) { return trunc_page(addr + get_page_size() - 1); }

	inline addr_t map_src_addr(addr_t core_local, addr_t phys) { return phys; }
	inline size_t constrain_map_size_log2(size_t size_log2) { return get_page_size_log2(); }


	inline void print_page_fault(const char *msg, addr_t pf_addr, addr_t pf_ip,
	                             Region_map::State::Fault_type pf_type,
	                             unsigned long faulter_badge)
	{
		printf("%s (%s pf_addr=%p pf_ip=%p from %02lx)\n", msg,
		       pf_type == Region_map::State::WRITE_FAULT ? "WRITE" : "READ",
		       (void *)pf_addr, (void *)pf_ip,
		       faulter_badge);
	}
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
