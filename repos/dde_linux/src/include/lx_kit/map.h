/*
 * \brief  Lx_kit associative data structure
 * \author Norman Feske
 * \date   2021-07-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__MAP_H_
#define _LX_KIT__MAP_H_

#include <base/stdint.h>

namespace Lx_kit {

	using namespace Genode;

	template <typename ITEM>
	struct Map;
}


template <typename ITEM>
class Lx_kit::Map : Noncopyable
{
	private:

		struct Item : Avl_node<Item>
		{
			ITEM const value;

			template <typename QUERY>
			static Item *_lookup(Item *curr, QUERY const &query)
			{
				for (;;) {
					if (!curr)
						return nullptr;

					ITEM const &value = curr->value;

					if (query.matches(value))
						return curr;

					curr = curr->Avl_node<Item>::child(value.higher(query.key()));
				}
			}

			/**
			 * Avl_node interface
			 */
			bool higher(Item *other) const { return value.higher(other->value.key); }

			template <typename QUERY>
			Item *lookup(QUERY const &query) { return _lookup(this, query); }

			template <typename... ARGS>
			Item(ARGS &&... args) : value{args...} { }
		};

		Avl_tree<Item> _items { };

		Allocator &_alloc;

		template <typename QUERY>
		Item *_lookup(QUERY const &query)
		{
			return _items.first() ? _items.first()->lookup(query) : nullptr;
		}

	public:

		Map(Allocator &alloc) : _alloc(alloc) { }

		template <typename... ARGS>
		void insert(ARGS &&... args)
		{
			_items.insert(new (_alloc) Item(args...));
		}

		template <typename QUERY>
		void remove(QUERY const &query)
		{
			while (Item *item_ptr = _lookup(query)) {
				_items.remove(item_ptr);
				destroy(_alloc, item_ptr);
			}
		}

		template <typename QUERY, typename FN>
		void apply(QUERY const &query, FN const &fn)
		{
			Item *item_ptr = _lookup(query);
			if (item_ptr)
				fn(item_ptr->value);
		}
};

#endif /* _LX_KIT__MAP_H_ */
