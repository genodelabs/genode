/*
 * \brief  Interface for process-local capability-selector allocation
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-19
 *
 * This interface is Fiasco-specific and not part of the Genode API. It should
 * only be used internally by the framework or by Fiasco-specific code. The
 * implementation of the interface is part of the environment library.
 *
 * This implementation is borrowed by the nova-platform equivalent.
 * (TODO: merge it)
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CAP_SEL_ALLOC_H_
#define _INCLUDE__BASE__CAP_SEL_ALLOC_H_

#include <base/stdint.h>

namespace Genode
{
	class Capability_allocator
	{
		private:

			addr_t _cap_idx;

			/**
			 * Constructor
			 */
			Capability_allocator();

		public:

			/**
			 * Return singleton instance of 'Capability_allocator'
			 */
			static Capability_allocator* allocator();

			/**
			 * Allocate range of capability selectors
			 *
			 * \param num_caps_log2  number of capability selectors. By default,
			 *                       the function returns a single capability selector.
			 * \return               first capability selector of allocated range,
			 *                       or 0 if allocation failed
			 */
			addr_t alloc(size_t num_caps = 1);

			/**
			 * Release range of capability selectors
			 *
			 * \param cap            first capability selector of range
			 * \param num_caps_log2  number of capability selectors to free.
			 */
			void free(addr_t cap, size_t num_caps = 1);
	};
}

#endif /* _INCLUDE__BASE__CAP_SEL_ALLOC_H_ */

