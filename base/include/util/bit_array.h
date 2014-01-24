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

#ifndef _INCLUDE__UTIL__BIT_ARRAY_H_
#define _INCLUDE__UTIL__BIT_ARRAY_H_

#include <base/exception.h>
#include <base/stdint.h>

namespace Genode {

	template <unsigned BITS>
	class Bit_array
	{
		public:

			class Invalid_index_access : public Exception {};
			class Invalid_clear        : public Exception {};
			class Invalid_set          : public Exception {};

		private:

			static constexpr size_t _BITS_PER_BYTE = 8UL;
			static constexpr size_t _BITS_PER_WORD = sizeof(addr_t) *
			                                        _BITS_PER_BYTE;
			static constexpr size_t _WORDS         = BITS / _BITS_PER_WORD;

			static_assert(BITS % _BITS_PER_WORD == 0,
			              "Count of bits need to be word aligned!");

			addr_t _words[_WORDS];

			addr_t _word(addr_t index) const {
				return index / _BITS_PER_WORD; }

			void _check_range(addr_t const index,
			                  addr_t const width) const
			{
				if ((index >= _WORDS * _BITS_PER_WORD) ||
				    width > _WORDS * _BITS_PER_WORD ||
				    _WORDS * _BITS_PER_WORD - width < index)
					throw Invalid_index_access();
			}

			addr_t _mask(addr_t const index, addr_t const width,
			             addr_t &rest) const
			{
				addr_t const shift = index - _word(index) * _BITS_PER_WORD;

				rest = width + shift > _BITS_PER_WORD ?
				       width + shift - _BITS_PER_WORD : 0;

				return (width >= _BITS_PER_WORD) ? ~0UL << shift
				                                : ((1UL << width) - 1) << shift;
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
							throw Invalid_clear();
						_words[word] &= ~mask;
					} else {
						if (_words[word] & mask)
							throw Invalid_set();
						_words[word] |= mask;
					}

					index = (_word(index) + 1) * _BITS_PER_WORD;
					width = rest;
				} while (rest);
			}

		public:

			Bit_array() { memset(&_words, 0, sizeof(_words)); }

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

			void set(addr_t const index, addr_t const width) {
				_set(index, width, false); }

			void clear(addr_t const index, addr_t const width) {
				_set(index, width, true); }
	};

}
#endif /* _INCLUDE__UTIL__BIT_ARRAY_H_ */
