/*
 * \brief  Child alias
 * \author Norman Feske
 * \date   2026-03-10
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ALIAS_H_
#define _ALIAS_H_

#include "child.h"

struct Depot_deploy::Alias : List_model<Alias>::Element,
                             Alias_dict::Element
{
	using Name = Child::Name;

	Name to_child { };

	static Name node_name(Node const &node)
	{
		return node.attribute_value("name", Name());
	}

	Alias(Alias_dict &dict, Name const &name) : Alias_dict::Element(dict, name) { };

	/**
	 * List_model::Element
	 */
	bool matches(Node const &node) const { return node_name(node) == name; }

	/**
	 * List_model::Element
	 */
	static bool type_matches(Node const &node) { return node.has_type("alias"); }

	static Progress update_from_node(List_model<Alias> &aliases, Alias_dict &dict,
	                                 Allocator &alloc, Node const &node)
	{
		Progress result { };
		aliases.update_from_node(node,

			/* create */
			[&] (Node const &node) -> Alias & {
				result = PROGRESSED;
				return *new (alloc) Alias(dict, node_name(node)); },

			/* destroy */
			[&] (Alias &alias) {
				result = PROGRESSED;
				destroy(alloc, &alias); },

			/* update */
			[&] (Alias &alias, Node const &node) {
				using Name = Child::Name;
				Name const to_child = node.attribute_value("child", Name());
				if (to_child != alias.to_child) {
					alias.to_child = to_child;
					result = PROGRESSED;
				}
			}
		);
		return result;
	}
};


static inline void Depot_deploy::with_server_name(Child_name const &child_or_alias,
                                                  Alias_dict const &alias_dict,
                                                  auto       const &fn)
{
	alias_dict.with_element(child_or_alias,
		[&] (Alias const &alias) { fn(alias.to_child); },
		[&]                      { fn(child_or_alias); });
}

#endif /* _ALIAS_H_ */
