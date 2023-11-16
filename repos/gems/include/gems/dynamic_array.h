/*
 * \brief  Dynamically growing array
 * \author Norman Feske
 * \date   2020-01-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__DYNAMIC_ARRAY_H_
#define _INCLUDE__GEMS__DYNAMIC_ARRAY_H_

#include <base/allocator.h>

namespace Genode { template <typename> struct Dynamic_array; }


template <typename ET>
struct Genode::Dynamic_array
{
	public:

		struct Index { unsigned value; };

	private:

		Allocator &_alloc;

		using Element = Constructible<ET>;

		Element *_array = nullptr;

		size_t   _capacity    = 0;
		unsigned _upper_bound = 0; /* index after last used element */

		bool _index_valid(Index at) const
		{
			return (at.value < _upper_bound) && _array[at.value].constructed();
		}

		/*
		 * Noncopyable
		 */
		Dynamic_array(Dynamic_array const &other);
		void operator = (Dynamic_array const &);

	public:

		/**
		 * Moving constructor
		 */
		Dynamic_array(Dynamic_array &other)
		:
			_alloc(other._alloc), _array(other._array),
			_capacity(other._capacity), _upper_bound(other._upper_bound)
		{
			other._array       = nullptr;
			other._capacity    = 0;
			other._upper_bound = 0;
		}

		Dynamic_array(Allocator &alloc) : _alloc(alloc) { }

		~Dynamic_array()
		{
			if (!_array)
				return;

			clear();

			_alloc.free(_array, _capacity*sizeof(Element));
		}

		void clear()
		{
			if (_upper_bound > 0)
				for (unsigned i = _upper_bound; i > 0; i--)
					destruct(Index{i - 1});
		}

		void insert(Index at, auto &&... args)
		{
			/* grow array if index exceeds current capacity or if it's full */
			if (at.value >= _capacity || _upper_bound == _capacity) {

				size_t const new_capacity =
					2 * max(_capacity, max(8U, at.value));

				_alloc.try_alloc(sizeof(Element)*new_capacity).with_result(

					[&] (void *ptr) {

						Element *new_array = (Element *)ptr;

						for (unsigned i = 0; i < new_capacity; i++)
							construct_at<Element>(&new_array[i]);

						if (_array) {
							for (unsigned i = 0; i < _upper_bound; i++)
								new_array[i].construct(*_array[i]);

							_alloc.free(_array, sizeof(Element)*_capacity);
						}

						_array    = new_array;
						_capacity = new_capacity;
					},
					[&] (Allocator::Alloc_error e) {
						Allocator::throw_alloc_error(e); }
				);
			}

			/* make room for new element */
			if (_upper_bound > 0)
				for (unsigned i = _upper_bound; i > at.value; i--)
					_array[i].construct(*_array[i - 1]);

			_array[at.value].construct(args...);

			_upper_bound = max(at.value + 1, _upper_bound + 1);
		}

		void append(auto &&... args) { insert(Index{_upper_bound}, args...); }

		bool exists(Index at) const { return _index_valid(at); }

		Index upper_bound() const { return Index { _upper_bound }; }

		void destruct(Index at)
		{
			if (!_index_valid(at))
				return;

			_array[at.value].destruct();

			if (_upper_bound > 0)
				for (unsigned i = at.value; i < _upper_bound - 1; i++)
					_array[i].construct(*_array[i + 1]);

			_upper_bound--;
			_array[_upper_bound].destruct();
		}

		void apply(Index at, auto const &fn)
		{
			if (_index_valid(at))
				fn(*_array[at.value]);
		}

		void apply(Index at, auto const &fn) const
		{
			if (_index_valid(at))
				fn(*_array[at.value]);
		}

		struct Range { Index at; unsigned length; };

		void for_each(Range range, auto const &fn) const
		{
			unsigned const first = range.at.value;
			unsigned const limit = min(_upper_bound, first + range.length);

			for (unsigned i = first; i < limit; i++)
				if (_array[i].constructed())
					fn(Index{i}, *_array[i]);
		}

		void for_each(auto const &fn) const
		{
			for_each(Range { .at = { 0U }, .length = ~0U }, fn);
		}

		void print(Output &out) const
		{
			for (unsigned i = 0; i < _upper_bound; i++)
				if (_array[i].constructed())
					Genode::print(out, *_array[i]);
		}
};

#endif /* _INCLUDE__GEMS__DYNAMIC_ARRAY_H_ */
