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

#include <util/attempt.h>
#include <base/log.h>

namespace Genode {

	class Bit_array_base;
	template <unsigned> class Bit_array;
}


class Genode::Bit_array_base
{
	protected:

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE
		};

	private:

		unsigned const _bit_cnt;
		unsigned const _word_cnt = _bit_cnt / BITS_PER_WORD;
		addr_t * const _words;

		addr_t _word(addr_t index) const { return index / BITS_PER_WORD; }

		bool _range_valid(addr_t const index, addr_t const width) const
		{
			unsigned const num_bits = _word_cnt*BITS_PER_WORD;
			bool     const conflict = (index >= num_bits)
			                       || (width >  num_bits)
			                       || (num_bits - width < index);
			return !conflict;
		}

		addr_t _mask(addr_t const index, addr_t const width, addr_t &remain) const
		{
			addr_t const shift = index - _word(index) * BITS_PER_WORD;

			remain = width + shift > BITS_PER_WORD ?
			         width + shift - BITS_PER_WORD : 0;

			return (width >= BITS_PER_WORD) ? ~0UL << shift
			                                : ((1UL << width) - 1) << shift;
		}

		/**
		 * \return true on success
		 */
		[[nodiscard]] bool _set(addr_t index, addr_t width, bool free)
		{
			if (!_range_valid(index, width))
				return false;

			addr_t remain;
			do {
				addr_t const word = _word(index);
				addr_t const mask = _mask(index, width, remain);

				if (free) {
					if ((_words[word] & mask) != mask) {
						error("Bit_array: invalid clear");
						return false;
					}
					_words[word] &= ~mask;
				} else {
					if (_words[word] & mask) {
						error("Bit_array: invalid set");
						return false;
					}
					_words[word] |= mask;
				}

				index = (_word(index) + 1) * BITS_PER_WORD;
				width = remain;
			} while (remain);

			return true;
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

		enum class Error { DENIED };

		/**
		 * Return true if at least one bit is set between
		 * index until index + width - 1
		 */
		Attempt<bool, Error> get(addr_t index, addr_t width) const
		{
			if (!_range_valid(index, width))
				return Error::DENIED;

			bool used = false;
			addr_t remain, mask;
			do {
				mask  = _mask(index, width, remain);
				used  = _words[_word(index)] & mask;
				index = (_word(index) + 1) * BITS_PER_WORD;
				width = remain;
			} while (!used && remain);

			return used;
		}

		Attempt<Ok, Error> set(addr_t const index, addr_t const width)
		{
			if (_set(index, width, false))
				return Ok();

			return Error::DENIED;
		}

		Attempt<Ok, Error> clear(addr_t const index, addr_t const width)
		{
			if (_set(index, width, true))
				return Ok();

			return Error::DENIED;
		}
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
