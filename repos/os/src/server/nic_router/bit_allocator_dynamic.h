/*
 * \brief  Bit Allocator that uses a dynamically allocated RAM dataspace
 * \author Martin Stein
 * \date   2017-09-26
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BIT_ALLOCATOR_DYNAMIC_H_
#define _BIT_ALLOCATOR_DYNAMIC_H_

/* Genode includes */
#include <base/allocator.h>

namespace Genode {

	class Bit_array_dynamic;
	class Bit_allocator_dynamic;
}


class Genode::Bit_array_dynamic
{
	public:

		struct Invalid_bit_count              : Exception { };
		struct Invalid_index_access           : Exception { };
		struct Invalid_clear                  : Exception { };
		struct Invalid_set                    : Exception { };
		struct Count_of_bits_not_word_aligned : Exception { };

	protected:

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE
		};

	private:

		unsigned _bit_cnt;
		unsigned _word_cnt;
		addr_t  *_words;

		addr_t _word(addr_t index) const {
			return index / BITS_PER_WORD; }

		void _check_range(addr_t const index,
		                  addr_t const width) const
		{
			if ((index >= _word_cnt * BITS_PER_WORD) ||
			    width > _word_cnt * BITS_PER_WORD ||
			    _word_cnt * BITS_PER_WORD - width < index)
				throw Invalid_index_access();
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

				index = (_word(index) + 1) * BITS_PER_WORD;
				width = rest;
			} while (rest);
		}

	public:

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

		Bit_array_dynamic(addr_t *addr, unsigned bits)
		: _bit_cnt(bits), _word_cnt(_bit_cnt / BITS_PER_WORD),
		  _words(addr)
		{
			if (!bits || bits % BITS_PER_WORD)
				throw Invalid_bit_count();

			memset(_words, 0, sizeof(addr_t)*_word_cnt);

			if (bits % BITS_PER_WORD)
				throw Count_of_bits_not_word_aligned();
		}
};


class Genode::Bit_allocator_dynamic
{
	private:

		/*
		 * Noncopyable
		 */
		Bit_allocator_dynamic(Bit_allocator_dynamic const &);
		Bit_allocator_dynamic &operator = (Bit_allocator_dynamic const &);

		addr_t             _next { 0 };
		Allocator         &_alloc;
		unsigned    const  _bits_aligned;
		addr_t     *const  _ram;
		Bit_array_dynamic  _array;

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE,
		};

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

		size_t _ram_size() const
		{
			return (_bits_aligned / BITS_PER_WORD) * sizeof(addr_t);
		}

	public:

		struct Out_of_indices : Exception { };
		struct Range_conflict : Exception { };

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
				} catch (Bit_array_dynamic::Invalid_index_access) { }

				max = _next;
				_next = 0;

			} while (max != 0);

			throw Out_of_indices();
		}

		void alloc_addr(addr_t const bit_start, size_t const num_log2 = 0)
		{
			addr_t const step = 1UL << num_log2;
			if (_array.get(bit_start, step))
				throw Range_conflict();

			_array.set(bit_start, step);
			_next = bit_start + step;
			return;
		}

		void free(addr_t const bit_start, size_t const num_log2 = 0)
		{
			_array.clear(bit_start, 1UL << num_log2);
			_next = bit_start;
		}

		Bit_allocator_dynamic(Allocator &alloc, unsigned bits)
		:
			_alloc(alloc),
			_bits_aligned(bits % BITS_PER_WORD ?
			              bits + BITS_PER_WORD - (bits % BITS_PER_WORD) :
			              bits),
			_ram((addr_t *)_alloc.alloc(_ram_size())),
			_array(_ram, _bits_aligned)
		{
			_reserve(bits, _bits_aligned - bits);
		}

		~Bit_allocator_dynamic()
		{
			_alloc.free((void *)_ram, _ram_size());
		}
};

#endif /* _BIT_ALLOCATOR_DYNAMIC_H_ */
