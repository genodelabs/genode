/*
 * \brief  ARM Cortex A8 CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__CORTEX_A8__CPU_H_
#define _CORE__INCLUDE__SPEC__CORTEX_A8__CPU_H_

/* core includes */
#include <spec/arm_v7/cpu_support.h>

namespace Genode { class Cpu; }


class Genode::Cpu : public Arm_v7
{
	public:

		/**
		 * Write back dirty cache lines and invalidate the whole cache
		 */
		void clean_invalidate_data_cache() {
			clean_invalidate_inner_data_cache(); }

		/**
		 * Invalidate all cache lines
		 */
		void invalidate_data_cache() {
			invalidate_inner_data_cache(); }

		/**
		 * Ensure that TLB insertions get applied
		 */
		void translation_table_insertions();

		/**
		 * Post processing after a translation was added to a translation table
		 *
		 * \param addr  virtual address of the translation
		 * \param size  size of the translation
		 */
		static void translation_added(addr_t const addr, size_t const size);
};

#endif /* _CORE__INCLUDE__SPEC__CORTEX_A8__CPU_H_ */
