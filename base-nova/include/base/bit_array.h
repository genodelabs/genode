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

#ifndef _INCLUDE__BASE__BIT_ARRAY_H_
#define _INCLUDE__BASE__BIT_ARRAY_H_

#include <base/stdint.h>

namespace Genode {

	class Bit_array_invalid_index_access{};
	class Bit_array_invalid_clear{};
	class Bit_array_invalid_set{};
	class Bit_array_out_of_indexes{};

	template <addr_t WORDS>
	class Bit_array
	{
		private:

			enum {
				_BITS_PER_BYTE = 8UL,
				_BITS_PER_WORD = sizeof(addr_t) * _BITS_PER_BYTE,
			};

			addr_t _words[WORDS];

			addr_t _word(addr_t index) const
			{
				return index / _BITS_PER_WORD;
			}

			void _check_range(addr_t const index,
			                  addr_t const width) const
			{
				if ((index >= WORDS * _BITS_PER_WORD) ||
				    width > WORDS * _BITS_PER_WORD ||
				    WORDS * _BITS_PER_WORD - width < index)
					throw Bit_array_invalid_index_access();
			}

			addr_t _mask(addr_t const index, addr_t const width,
			             addr_t &rest) const
			{
				addr_t const shift = index - _word(index) * _BITS_PER_WORD;

				rest = width + shift > _BITS_PER_WORD ?
				       width + shift - _BITS_PER_WORD : 0;

				if (width >= _BITS_PER_WORD)
					return ~0UL << shift;
				else
					return ((1UL << width) - 1) << shift;

			}

			void _set(addr_t index, addr_t width, bool free)
			{
				_check_range(index, width);

				addr_t rest, word, mask;
				do {
					word = _word(index);
					mask = _mask(index, width, rest);

					if (free) {
						if ((_words[word] & mask) != mask)
							throw Bit_array_invalid_clear();
						_words[word] &= ~mask;
					} else {
						if (_words[word] & mask)
							throw Bit_array_invalid_set();
						_words[word] |= mask;
					}

					index = (_word(index) + 1) * _BITS_PER_WORD;
					width = rest;

				} while (rest);
			}

		public:

			Bit_array()
			{
				for (addr_t i = 0; i < WORDS; i++) _words[i] = 0UL;
			}

			/**
			 * Return true if at least one bit is set between
			 * index until index + width - 1
			 */
			bool get(addr_t index, addr_t width) const
			{
				_check_range(index, width);

				bool used = false;
				addr_t rest, mask;
				do {
					mask  = _mask(index, width, rest);
					used  = _words[_word(index)] & mask;
					index = (_word(index) + 1) * _BITS_PER_WORD;
					width = rest;
				} while (!used && rest);

				return used;
			}

			void set(addr_t const index, addr_t const width)
			{
				_set(index, width, false);
			}

			void clear(addr_t const index, addr_t const width)
			{
				_set(index, width, true);
			}
	};

}
#endif /* _INCLUDE__BASE__BIT_ARRAY_H_ */
