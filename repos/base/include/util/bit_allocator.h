/*
 * \brief  Allocator using bitmaps
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \date   2012-06-14
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		addr_t _next = 0;
		Array  _array { };

		/**
		 * Reserve consecutive number of bits
		 *
		 * \return true on success
		 * \noapi
		 */
		[[nodiscard]] bool _reserve(addr_t bit_start, size_t const num)
		{
			if (!num) return true;

			return _array.set(bit_start, num).ok();
		}

	public:

		Bit_allocator() { (void)_reserve(BITS, BITS_ALIGNED - BITS); }
		Bit_allocator(Bit_allocator const &other) : _array(other._array) { }

		using Error = Array::Error;

		/**
		 * Allocate block of bits
		 *
		 * \param num_log2  2-based logarithm of size of block
		 *
		 * The requested block is allocated at the lowest available index in
		 * the bit array.
		 */
		Attempt<addr_t, Error> alloc(size_t const num_log2 = 0)
		{
			addr_t const step = 1UL << num_log2;
			addr_t max = ~0UL;

			do {
				for (addr_t i = _next & ~(step - 1); i < max; i += step) {

					bool occupied = false, done = false, denied = false;

					_array.get(i, step).with_result(
						[&] (bool any_bit_set) {
							if (any_bit_set) {
								occupied = true;
							} else {
								if (_array.set(i, step).ok()) {
									_next = i + step;
									done = true;
								} else {
									denied = true; /* unexpected */
								}
							}
						},
						[&] (Error) { denied = true; }
					);

					if (done)     return i;
					if (occupied) continue;
					if (denied)   break;
				}
				max = _next;
				_next = 0;

			} while (max != 0);

			return Error::DENIED;
		}

		/**
		 * Allocate specific block of bits
		 *
		 * \param first_bit  desired address of block
		 * \param num_log2   2-based logarithm of size of block
		 */
		Attempt<Ok, Error> alloc_addr(addr_t const bit_start, size_t const num_log2 = 0)
		{
			addr_t const step = 1UL << num_log2;

			return _array.get(bit_start, step).template convert<Attempt<Ok, Error>>(

				[&] (bool any_bit_set) -> Attempt<Ok, Error> {
					if (any_bit_set)
						return Error::DENIED;

					auto const result = _array.set(bit_start, step);
					_next = bit_start + step;
					return result;
				},
				[&] (Error) { return Error::DENIED; });
		}

		void free(addr_t const bit_start, size_t const num_log2 = 0)
		{
			(void)_array.clear(bit_start, 1UL << num_log2);

			/*
			 * We only rewind the _next pointer (if needed) to densely allocate
			 * from the start of the array and avoid gaps.
			 */
			if (bit_start < _next)
				_next = bit_start;
		}
};

#endif /* _INCLUDE__UTIL__BIT_ALLOCATOR_H_ */
