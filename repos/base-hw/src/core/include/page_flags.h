/*
 * \brief   Generic page flags
 * \author  Stefan Kalkowski
 * \date    2014-02-24
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PAGE_FLAGS_H_
#define _CORE__INCLUDE__PAGE_FLAGS_H_

#include <base/cache.h>

namespace Genode
{
	/**
	 * Map app-specific mem attributes to a TLB-specific POD
	 */
	struct Page_flags
	{
		bool            writeable;
		bool            executable;
		bool            privileged;
		bool            global;
		bool            device;
		Cache_attribute cacheable;

		/**
		 * Create flag POD for Genode pagers
		 */
		static const Page_flags
		apply_mapping(bool const writeable,
		              Cache_attribute const cacheable,
		              bool const io_mem) {
			return Page_flags { writeable, true, false, false,
				                io_mem, cacheable }; }

		/**
		 * Create flag POD for the mode transition region
		 */
		static const Page_flags mode_transition() {
			return Page_flags { true, true, true, true, false, CACHED }; }
	};
}

#endif /* _CORE__INCLUDE__PAGE_FLAGS_H_ */
