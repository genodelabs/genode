/*
 * \brief  Core-internal utilities
 * \author Norman Feske
 * \date   2009-10-02
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

namespace Genode {

	constexpr size_t get_page_size_log2()       { return 12; }
	constexpr size_t get_page_size()            { return 1 << get_page_size_log2(); }
	constexpr addr_t get_page_mask()            { return ~(get_page_size() - 1); }
	constexpr size_t get_super_page_size_log2() { return 22; }
	constexpr size_t get_super_page_size()      { return 1 << get_super_page_size_log2(); }
	inline addr_t trunc_page(addr_t addr)    { return addr & get_page_mask(); }
	inline addr_t round_page(addr_t addr)    { return trunc_page(addr + get_page_size() - 1); }


	inline addr_t map_src_addr(addr_t core_local, addr_t phys) { return core_local; }


	inline size_t constrain_map_size_log2(size_t size_log2) { return size_log2; }


	inline void print_page_fault(const char *msg, addr_t pf_addr, addr_t pf_ip,
	                             Rm_session::Fault_type pf_type,
	                             unsigned long faulter_badge)
	{
		printf("%s (%s pf_addr=%p pf_ip=%p from %02lx %s)\n", msg,
		       pf_type == Rm_session::WRITE_FAULT ? "WRITE" : "READ",
		       (void *)pf_addr, (void *)pf_ip,
		       faulter_badge,
		       faulter_badge ? reinterpret_cast<char *>(faulter_badge) : 0);
	}


	inline void backtrace()
	{
		using namespace Genode;
		printf("\nbacktrace\n");
		printf(" %p\n", __builtin_return_address(0));
		printf(" %p\n", __builtin_return_address(1));
		printf(" %p\n", __builtin_return_address(2));
		printf(" %p\n", __builtin_return_address(3));
		printf(" %p\n", __builtin_return_address(4));
	}


	inline void hexdump(void *addr)
	{
		unsigned char *s = (unsigned char *)addr;
		printf("\nhexdump at 0x%p:\n", addr);
		for (unsigned j = 0; j < 4; j++) {
			printf("  ");
			for (unsigned i = 0; i < 16; i++)
				printf("0x%02x ", s[j*16 + i]);
			printf("\n");
		}
	}
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
