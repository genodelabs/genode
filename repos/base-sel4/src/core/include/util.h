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
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
