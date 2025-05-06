/*
 * \brief  Block session map
 * \author Josef Soentgen
 * \date   2025-05-06
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK__SESSION_MAP_H_
#define _INCLUDE__BLOCK__SESSION_MAP_H_

/* Genode includes */
#include <base/stdint.h>
#include <util/attempt.h>

namespace Block {

	using namespace Genode;

	template <typename T = uint8_t , uint32_t N = 32u>
	struct Session_map
	{
		static_assert(sizeof (T) <= sizeof (uint32_t));

		static constexpr uint32_t _bits  = (sizeof (addr_t) * 8);
		static constexpr uint32_t _mask  = _bits - 1;
		static constexpr uint32_t _count = N / _bits ? : 1u;

		addr_t _item[_count];

		void _set(uint32_t value)
		{
			uint32_t const i      = value / _bits;
			uint32_t const offset = value & _mask;

			_item[i] |= (1ul << offset);
		}

		bool _used(uint32_t value) const
		{
			uint32_t const i      = value / _bits;
			uint32_t const offset = value & _mask;
			return _item[i] & (1ul << offset);
		}

		uint32_t _find_free() const
		{
			for (uint32_t i = 0; i < N; i++)
				if (!_used(i))
					return i;

			return N;
		}

		void _for_each_used(auto const &fn) const
		{
			for (uint32_t i = 0; i < N; i++)
				if (_used(i))
					fn(Index { .value = (T)i });
		}

		Session_map()
		{
			for (addr_t & v : _item)
				v = 0;
		}

		uint32_t capacity() const { return N; }

		struct Index
		{
			T value;

			static Index from_id(unsigned long id) {
				return Index { .value = (T)id }; }
		};

		bool used(Index index) const
		{
			if (index.value >= N)
				return false;

			return _used(index.value);
		}

		struct Alloc_ok { Index index; };
		struct Alloc_error { };
		using Alloc_result = Attempt<Alloc_ok, Alloc_error>;

		Alloc_result alloc()
		{
			uint32_t const value = _find_free();
			if (value == N)
				return Alloc_error();

			_set(value);
			return Alloc_ok { .index = Index { (T)value } };
		}

		void free(Index index)
		{
			if (index.value >= N)
				return;

			uint32_t const i      = index.value / _bits;
			uint32_t const offset = index.value & _mask;
			_item[i] &= ~(1ul << offset);
		}

		T _first_id = 0;
		Index _idx_array[N] { };

		void for_each_index(auto const &fn)
		{
			T max_ids = 0;
			_for_each_used([&] (Session_map::Index idx) {
				_idx_array[max_ids] = idx;
				++max_ids;
			});

			for (T i = 0; i < max_ids; i++) {
				Index const idx = _idx_array[(_first_id + i) % max_ids];
				fn(idx);
			}
			_first_id++;
		}
	};
}

#endif /* _INCLUDE__BLOCK__SESSION_MAP_H_ */
