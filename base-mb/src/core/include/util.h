/*
 * \brief  Core-internal utilities
 * \author Martin Stein
 * \date   2010-09-08
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__INCLUDE__UTIL_H_
#define _SRC__CORE__INCLUDE__UTIL_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <base/printf.h>

/* Kernel includes */
#include <kernel/syscalls.h>

namespace Genode {

	inline size_t get_page_size_log2()    { return 12; }
	inline size_t get_page_size()         { return 1 << get_page_size_log2(); }
	inline addr_t get_page_mask()         { return ~(get_page_size() - 1); }
	inline addr_t trunc_page(addr_t addr) { return addr & get_page_mask(); }
	inline addr_t round_page(addr_t addr) { return trunc_page(addr + get_page_size() - 1); }

	inline addr_t map_src_addr(addr_t core_local, addr_t phys) { return phys; }
	inline size_t constrain_map_size_log2(size_t size_log2)
	{
		if (size_log2<14) return 12;
		if (size_log2<16) return 14;
		if (size_log2<18) return 16;
		if (size_log2<20) return 18;
		if (size_log2<22) return 20;
		if (size_log2<24) return 22;
		return 24;
	}

	inline void print_page_fault(const char *msg, addr_t pf_addr, addr_t pf_ip,
	                             Rm_session::Fault_type pf_type,
	                             unsigned long faulter_badge)
	{
		printf("%s (%s pf_addr=%p pf_ip=%p from %02lx)\n", msg,
		       pf_type == Rm_session::WRITE_FAULT ? "WRITE" : "READ",
		       (void *)pf_addr, (void *)pf_ip,
		       faulter_badge);
	}
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
