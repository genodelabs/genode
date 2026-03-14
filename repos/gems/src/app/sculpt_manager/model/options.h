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

#ifndef _MODEL__OPTIONS_H_
#define _MODEL__OPTIONS_H_

/* local includes */
#include <model/file_listing.h>

namespace Sculpt {

	struct Options : File_listing { using File_listing::File_listing; };

	class Enabled_options;
}


class Sculpt::Enabled_options
{
	private:

		Allocator &_alloc;

		struct Item;

		using Name = Options::Name;
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
				return node.has_type("option");
			}
		};

		List_model<Item> _items { };

		Dict _dict { }; /* avoid iterating over '_items' in 'exists' */

	public:

		Enabled_options(Allocator &alloc) : _alloc(alloc) { }

		~Enabled_options() { update_from_deploy(Node()); }

		Progress update_from_deploy(Node const &deploy)
		{
			bool progress = false;
			_items.update_from_node(deploy,

				/* create */
				[&] (Node const &node) -> Item & {
					progress = true;
					return *new (_alloc) Item(_dict, node_name(node)); },

				/* destroy */
				[&] (Item &i) {
					progress = true;
					destroy(_alloc, &i); },

				/* update */
				[&] (Item &, Node const &) { }
			);
			return Progress { progress };
		}

		bool exists(Name const &name) const { return _dict.exists(name); }
};

#endif /* _MODEL__OPTIONS_H_ */
