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

#ifndef _INCLUDE__UTIL__BIT_ARRAY_H_
#define _INCLUDE__UTIL__BIT_ARRAY_H_

#include <base/exception.h>
#include <base/error.h>
#include <base/log.h>

namespace Genode {

	class Bit_array_base;
	template <unsigned> class Bit_array;
}


class Genode::Bit_array_base
{
	public:

		using Invalid_index_access = Genode::Index_out_of_bounds;

	protected:

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE
		};

	private:

		unsigned const _bit_cnt;
		unsigned const _word_cnt = _bit_cnt / BITS_PER_WORD;
		addr_t * const _words;

		addr_t _word(addr_t index) const {
			return index / BITS_PER_WORD; }

		void _check_range(addr_t const index,
		                  addr_t const width) const
		{
			if ((index >= _word_cnt * BITS_PER_WORD) ||
			    width > _word_cnt * BITS_PER_WORD ||
			    _word_cnt * BITS_PER_WORD - width < index)
				raise(Unexpected_error::INDEX_OUT_OF_BOUNDS);
		}

		addr_t _mask(addr_t const index, addr_t const width,
		             addr_t &rest) const
		{
			addr_t const shift = index - _word(index) * BITS_PER_WORD;

			rest = width + shift > BITS_PER_WORD ?
			       width + shift - BITS_PER_WORD : 0;

			return (width >= BITS_PER_WORD) ? ~0UL << shift
			                                : ((1UL << width) - 1) << shift;
		}

		void _set(addr_t index, addr_t width, bool free)
		{
			_check_range(index, width);

			addr_t rest;
			do {
				addr_t const word = _word(index);
				addr_t const mask = _mask(index, width, rest);

				if (free) {
					if ((_words[word] & mask) != mask) {
						error("Bit_array: invalid clear");
						return;
					}
					_words[word] &= ~mask;
				} else {
					if (_words[word] & mask) {
						error("Bit_array: invalid set");
						return;
					}
					_words[word] |= mask;
				}

				index = (_word(index) + 1) * BITS_PER_WORD;
				width = rest;
			} while (rest);
		}

		/*
		 * Noncopyable
		 */
		Bit_array_base(Bit_array_base const &);
		Bit_array_base &operator = (Bit_array_base const &);

	public:

		/**
		 * Constructor
		 *
		 * \param ptr  pointer to array used as backing store for the bits.
		 *             The array must be initialized with zeros.
		 */
		Bit_array_base(unsigned bits, addr_t *ptr)
		:
			_bit_cnt(bits), _words(ptr)
		{
			if (!bits || bits % BITS_PER_WORD)
				error("Bit_array: invalid bit count");
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
				index = (_word(index) + 1) * BITS_PER_WORD;
				width = rest;
			} while (!used && rest);

			return used;
		}

		void set(addr_t const index, addr_t const width) {
			_set(index, width, false); }

		void clear(addr_t const index, addr_t const width) {
			_set(index, width, true); }
};


template <unsigned BITS>
class Genode::Bit_array : public Bit_array_base
{
	private:

		static constexpr size_t _WORDS = BITS / BITS_PER_WORD;

		static_assert(BITS % BITS_PER_WORD == 0,
		              "Count of bits need to be word aligned!");

		struct Array { addr_t values[_WORDS]; } _array { };

	public:

		Bit_array() : Bit_array_base(BITS, _array.values) { }

		Bit_array(Bit_array const &other)
		:
			Bit_array_base(BITS, _array.values), _array(other._array)
		{ }
};

#endif /* _INCLUDE__UTIL__BIT_ARRAY_H_ */
