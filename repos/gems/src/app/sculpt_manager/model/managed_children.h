/*
 * \brief  Cached child-management policies
 * \author Norman Feske
 * \date   2026-04-02
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__MANAGED_CHILDREN_H_
#define _MODEL__MANAGED_CHILDREN_H_

/* local includes */
#include <types.h>

namespace Sculpt { struct Managed_children; }


struct Sculpt::Managed_children
{
	struct Child;

	using Name = Start_name;
	using Dict = Dictionary<Child, Name>;

	static Name node_name(Node const &node)
	{
		return node.attribute_value("name", Name());
	}

	struct Attr
	{
		unsigned long max_ram, max_caps;

		static Attr from_node(Node const &node)
		{
			return {
				.max_ram  = node.attribute_value("max_ram", Number_of_bytes()),
				.max_caps = node.attribute_value("max_caps", 0UL)
			};
		}

		bool operator != (Attr const &other) const
		{
			return max_ram  != other.max_ram
			    || max_caps != other.max_caps;
		}
	};

	struct Child : Dict::Element, List_model<Child>::Element
	{
		using Dict::Element::Element;

		Attr attr { };

		bool matches(Node const &node) const
		{
			return node_name(node) == name;
		}

		static bool type_matches(Node const &node)
		{
			return node.has_type("child") || node.has_sub_node("managed");
		}
	};

	List_model<Child> _list { };

	Progress update_from_deploy_or_option(Allocator &alloc, Dict &dict, Node const &node)
	{
		bool progress = false;
		_list.update_from_node(node,

			/* create */
			[&] (Node const &node) -> Child & {
				progress = true;
				return *new (alloc) Child(dict, node_name(node)); },

			/* destroy */
			[&] (Child &c) {
				progress = true;
				destroy(alloc, &c); },

			/* update */
			[&] (Child &c, Node const &node)
			{
				Attr const orig = c.attr;
				node.with_optional_sub_node("managed", [&] (Node const &node) {
					c.attr = Attr::from_node(node); });
				if (orig != c.attr)
					progress = true;
			}
		);
		return Progress { progress };
	}
};

#endif /* _MODEL__MANAGED_CHILDREN_H_ */
