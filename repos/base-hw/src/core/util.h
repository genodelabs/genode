/*
 * \brief   Core-internal utilities
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-01-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__UTIL_H_
#define _CORE__UTIL_H_

/* core includes */
#include <types.h>
#include <hw/util.h>

namespace Core {

	using Hw::get_page_mask;
	using Hw::trunc_page;
	using Hw::round_page;
	using Hw::trunc;
	using Hw::aligned;
	using Hw::round;

	/**
	 * Select source used for map operations
	 */
	constexpr addr_t map_src_addr(addr_t, addr_t phys) { return phys; }

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
	static constexpr Log2 kernel_constrained_map_size(Log2 size)
	{
		return { (size.log2 < 20) ? uint8_t(12) : uint8_t(20) };
	}
}

#endif /* _CORE__UTIL_H_ */
