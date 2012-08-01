/*
 * \brief  Interface for process-local capability-selector allocation
 * \author Norman Feske
 * \date   2010-01-19
 *
 * This interface is NOVA-specific and not part of the Genode API. It should
 * only be used internally by the framework or by NOVA-specific code. The
 * implementation of the interface is part of the environment library.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CAP_SEL_ALLOC_H_
#define _INCLUDE__BASE__CAP_SEL_ALLOC_H_

#include <base/stdint.h>
#include <base/printf.h>
#include <base/bit_allocator.h>

namespace Genode {

	class Cap_selector_allocator : public Bit_allocator<4096> 
	{
		public:

			/**
			 * Constructor
			 */
			Cap_selector_allocator(); 

			/**
			 * Allocate range of capability selectors
			 *
			 * \param num_caps_log2  number of capability selectors specified as
			 *                       as the power of two. By default, the function
			 *                       returns a single capability selector.
			 * \return               first capability selector of allocated range,
			 *                       or 0 if allocation failed
			 *
			 * The allocated range will be naturally aligned according to the
			 * specified 'num_cap_log2' value.
			 */
			addr_t alloc(size_t num_caps_log2 = 0);

			/**
			 * Release range of capability selectors
			 *
			 * \param cap            first capability selector of range
			 * \param num_caps_log2  number of capability selectors specified
			 *                       as the power of two
			 */
			void free(addr_t cap, size_t num_caps_log2);

	};

	/**
	 * Return singleton instance of 'Cap_selector_allocator'
	 */
	Cap_selector_allocator *cap_selector_allocator();
}

#endif /* _INCLUDE__BASE__CAP_SEL_ALLOC_H_ */

