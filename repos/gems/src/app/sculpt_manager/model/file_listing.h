/*
 * \brief  Cached information about available options
 * \author Norman Feske
 * \date   2026-03-14
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__FILE_LISTING_H_
#define _MODEL__FILE_LISTING_H_

/* Genode includes */
#include <util/list_model.h>
#include <util/dictionary.h>

/* local includes */
#include <types.h>
#include <runtime.h>

namespace Sculpt { class File_listing; }


class Sculpt::File_listing : public Noncopyable
{
	public:

		using Name = Path;

	private:

		Allocator &_alloc;

		struct Item;

		using Dict = Dictionary<Item, Name>;

		static Name node_name(Node const &node)
		{
			return node.attribute_value("name", Name());
		}

		struct Item : Dict::Element, List_model<Item>::Element
		{
			using Dict::Element::Element;

			bool matches(Node const &node) const { return node_name(node) == name; }

			static bool type_matches(Node const &node)
			{
				return node.has_type("file");
			}
		};

		Dict _sorted { };

		List_model<Item> _items { };

	public:

		File_listing(Allocator &alloc) : _alloc(alloc) { }

		void update_from_node(Node const &files)
		{
			_items.update_from_node(files,

				/* create */
				[&] (Node const &node) -> Item & {
					return *new (_alloc) Item(_sorted, node_name(node)); },

				/* destroy */
				[&] (Item &i) { destroy(_alloc, &i); },

				/* update */
				[&] (Item &, Node const &) { }
			);
		}

		void for_each(auto const &fn) const
		{
			_sorted.for_each([&] (Dict::Element const &e) { fn(e.name); });
		}
};

#endif /* _MODEL__FILE_LISTING_H_ */
