/*
 * \brief   Core-internal utilities
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

/* base-internal includes */
#include <base/internal/page_size.h>

/* core includes */
#include <core_cspace.h>

namespace Core {

	inline addr_t trunc_page(addr_t addr) { return addr & PAGE_MASK; }
	inline addr_t round_page(addr_t addr) { return trunc_page(addr + PAGE_SIZE - 1); }

	inline addr_t map_src_addr(addr_t, addr_t phys) { return phys; }

	inline Log2 kernel_constrained_map_size(Log2 const size) {
		return Log2(min(size.log2, uint8_t(seL4_LargePageBits))); }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
