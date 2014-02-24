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

#ifndef _TLB__PAGE_FLAGS_H_
#define _TLB__PAGE_FLAGS_H_

namespace Genode
{
	/**
	 * Map app-specific mem attributes to a TLB-specific POD
	 */
	struct Page_flags
	{
		bool writeable;
		bool executable;
		bool privileged;
		bool global;
		bool device;
		bool cacheable;

		/**
		 * Create flag POD for Genode pagers
		 */
		static const Page_flags
		apply_mapping(bool const writeable,
		              bool const write_combined,
		              bool const io_mem) {
			return Page_flags { writeable, true, false, false,
				                io_mem, !write_combined && !io_mem }; }

		/**
		 * Create flag POD for kernel when it creates the core space
		 */
		static const Page_flags map_core_area(bool const io_mem) {
			return Page_flags { true, true, false, false, io_mem, !io_mem }; }

		/**
		 * Create flag POD for the mode transition region
		 */
		static const Page_flags mode_transition() {
			return Page_flags { true, true, true, true, false, true }; }
	};
}

#endif /* _TLB__PAGE_FLAGS_H_ */
