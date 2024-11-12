/*
 * \brief  Window layouter
 * \author Norman Feske
 * \date   2015-12-31
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <os/surface.h>
#include <util/list_model.h>

namespace Window_layouter {

	using namespace Genode;

	using Point = Surface_base::Point;
	using Area  = Surface_base::Area;
	using Rect  = Surface_base::Rect;

	struct Window_id
	{
		unsigned value = 0;

		Window_id() { }
		Window_id(unsigned value) : value(value) { }

		bool valid() const { return value != 0; }

		bool operator != (Window_id const &other) const
		{
			return other.value != value;
		}

		bool operator == (Window_id const &other) const
		{
			return other.value == value;
		}
	};

	class Window;

	using Name = String<64>;

	Name const name;

	static Name name_from_xml(Xml_node const &node)
	{
		return node.attribute_value("name", Name());
	}

	static inline void copy_attributes(Xml_generator &xml, Xml_node const &from)
	{
		using Value = String<64>;
		from.for_each_attribute([&] (Xml_attribute const &attr) {
			Value value { };
			attr.value(value);
			xml.attribute(attr.name().string(), value);
		});
	}

	struct Xml_max_depth { unsigned value; };

	static inline void copy_node(Xml_generator &xml, Xml_node const &from,
	                             Xml_max_depth max_depth = { 5 })
	{
		if (max_depth.value)
			xml.node(from.type().string(), [&] {
				copy_attributes(xml, from);
				from.for_each_sub_node([&] (Xml_node const &sub_node) {
					copy_node(xml, sub_node, { max_depth.value - 1 }); }); });
	}
}

#endif /* _TYPES_H_ */
