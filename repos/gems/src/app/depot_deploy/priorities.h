/*
 * \brief  Priority names
 * \author Norman Feske
 * \date   2026-04-23
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PRIORITIES_H_
#define _PRIORITIES_H_

#include <util/list_model.h>
#include <util/dictionary.h>
#include <util/progress.h>

namespace Depot_deploy {

	using namespace Depot;

	struct Prio_levels
	{
		unsigned value;

		int min_priority() const
		{
			return (value > 0) ? -(int)(value - 1) : 0;
		}
	};

	struct Priorities;
}


struct Depot_deploy::Priorities
{
	struct Prio;

	using Name = String<16>;
	using Dict = Dictionary<Prio, Name>;

	Dict             _dict { };
	List_model<Prio> _list { };

	int _default = 0;

	struct Prio : List_model<Prio>::Element, Dict::Element
	{
		int value { };

		Prio(Dict &dict, Name const &name) : Dict::Element(dict, name) { };

		/**
		 * List_model::Element
		 */
		bool matches(Node const &node) const { return node.type() == name; }

		/**
		 * List_model::Element
		 */
		static bool type_matches(Node const &) { return true; }
	};

	Progress update_from_node(Prio_levels const &levels, Allocator &alloc, Node const &node)
	{
		_default = node.attribute_value("default", levels.min_priority());;

		Progress result { };
		_list.update_from_node(node,

			/* create */
			[&] (Node const &node) -> Prio & {
				result = PROGRESSED;
				return *new (alloc) Prio(_dict, node.type()); },

			/* destroy */
			[&] (Prio &p) {
				result = PROGRESSED;
				destroy(alloc, &p); },

			/* update */
			[&] (Prio &p, Node const &node) {
				int const orig = p.value;
				p.value = node.attribute_value("name", _default);
				if (orig != p.value)
					result = PROGRESSED;
			}
		);
		return result;
	}

	int from_child_attr(Node const &child) const
	{
		Name const name_or_number = child.attribute_value("priority", Name());
		return _dict.with_element(name_or_number,
			[&] (Prio const &p) { return p.value; },
			[&]                 { return _default; });
	}
};

#endif /* _PRIORITIES_H_ */
