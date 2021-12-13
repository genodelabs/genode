/*
 * \brief  Core-internal utilities
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/page_size.h>
#include <platform_thread.h>

namespace Genode {

	constexpr size_t get_super_page_size_log2() { return 22; }
	constexpr size_t get_super_page_size()      { return 1 << get_super_page_size_log2(); }

	template <typename T>
	inline T trunc_page(T addr) { return addr & _align_mask(get_page_size_log2()); }

	template <typename T>
	inline T round_page(T addr) { return trunc_page(addr + get_page_size() - 1); }

	inline addr_t map_src_addr(addr_t /* core_local */, addr_t phys) { return phys; }


	inline size_t constrain_map_size_log2(size_t size_log2)
	{
		/* Nova::Mem_crd order has 5 bits available and is in 4K page units */
		enum { MAX_MAP_LOG2 = (1U << 5) - 1 + 12 };
		return size_log2 > MAX_MAP_LOG2 ? (size_t)MAX_MAP_LOG2 : size_log2;
	}

	inline unsigned scale_priority(unsigned const prio, char const * name)
	{
		using Nova::Qpd;
		unsigned priority = Cpu_session::scale_priority(Qpd::DEFAULT_PRIORITY,
		                                                prio);
		if (priority == 0) {
			warning("priority of thread '", name, "' below minimum - boost to 1");
			priority = 1;
		}
		if (priority > Nova::Qpd::DEFAULT_PRIORITY) {
			warning("priority of thread '", name, "' above maximum - limit to ",
			        (unsigned)Qpd::DEFAULT_PRIORITY);
			priority = Qpd::DEFAULT_PRIORITY;
		}
		return priority;
	}
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
