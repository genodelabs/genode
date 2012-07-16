/*
 * \brief  Allocator using bitmaps to maintain cap space
 * \author Alexander Boettcher
 * \date   2012-06-14
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__BIT_ALLOCATOR_H_
#define _INCLUDE__BASE__BIT_ALLOCATOR_H_

#include <base/bit_array.h>

namespace Genode {

	template<addr_t WORDS>
	class Bit_allocator
	{
		protected:
			Bit_array<WORDS> _array;

			void _reserve(addr_t bit_start, size_t const num_cap)
			{
				if (!num_cap) return;

				_array.set(bit_start, num_cap);
			}

		public:

			addr_t alloc(size_t const num_log2)
			{
				addr_t const step = 1UL << num_log2;
				addr_t i = 0;

				try {
					/*
					 * Throws exception if array is
					 * accessed outside bounds
					 */
					while (true) {
						if (_array.get(i, step)) {
							i += step;
							continue;
						}
						_array.set(i, step);
						return i;
					}
				} catch (Bit_array_invalid_index_access) {}

				throw Bit_array_out_of_indexes();
			}

			void free(addr_t const bit_start,
			          size_t const num_log2)
			{
				_array.clear(bit_start, 1UL << num_log2);
			}

	};
}
#endif /* _INCLUDE__BASE__BIT_ALLOCATOR_H_ */
