/*
 * \brief  Allocator using bitmaps
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \date   2012-06-14
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__BIT_ALLOCATOR_H_
#define _INCLUDE__UTIL__BIT_ALLOCATOR_H_

#include <util/bit_array.h>

namespace Genode { template<unsigned> class Bit_allocator; } 


template<unsigned BITS>
class Genode::Bit_allocator
{
	protected:

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE,
			BITS_ALIGNED  = (BITS + BITS_PER_WORD - 1UL)
			                & ~(BITS_PER_WORD - 1UL),
		};

		using Array = Bit_array<BITS_ALIGNED>;

		addr_t _next;
		Array  _array;

		/**
		 * Reserve consecutive number of bits
		 *
		 * \noapi
		 */
		void _reserve(addr_t bit_start, size_t const num)
		{
			if (!num) return;

			_array.set(bit_start, num);
		}

	public:

		class Out_of_indices : Exception {};

		Bit_allocator() : _next(0) {
			_reserve(BITS, BITS_ALIGNED - BITS); }

		addr_t alloc(size_t const num_log2 = 0)
		{
			addr_t const step = 1UL << num_log2;
			addr_t max = ~0UL;

			do {
				try {
					/* throws exception if array is accessed outside bounds */
					for (addr_t i = _next & ~(step - 1); i < max; i += step) {
						if (_array.get(i, step))
							continue;

						_array.set(i, step);
						_next = i + step;
						return i;
					}
				} catch (typename Array::Invalid_index_access) { }

				max = _next;
				_next = 0;

			} while (max != 0);

			throw Out_of_indices();
		}

		void free(addr_t const bit_start, size_t const num_log2 = 0)
		{
			_array.clear(bit_start, 1UL << num_log2);
			_next = bit_start;
		}
};

#endif /* _INCLUDE__UTIL__BIT_ALLOCATOR_H_ */
