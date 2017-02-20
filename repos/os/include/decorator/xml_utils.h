/*
 * \brief  Utilities for XML parsing
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DECORATOR__XML_UTILS_H_
#define _INCLUDE__DECORATOR__XML_UTILS_H_

#include <decorator/types.h>


namespace Decorator {

	template <typename T>
	static T attribute(Xml_node const &, char const *, T);

	template <size_t CAPACITY>
	Genode::String<CAPACITY> string_attribute(Xml_node const &, char const *,
	                                          Genode::String<CAPACITY> const &);

	static Point point_attribute(Xml_node const &);

	static Area area_attribute(Xml_node const &);

	static Rect rect_attribute(Xml_node const &);

	template <typename FUNC>
	static void for_each_sub_node(Xml_node, char const *, FUNC const &);

	static Color color(Xml_node const &);
}


/**
 * Read attribute value from XML node
 *
 * \param node           XML node
 * \param name           attribute name
 * \param default_value  value returned if no such attribute exists
 */
template <typename T>
static T
Decorator::attribute(Xml_node const &node, char const *name, T default_value)
{
	T result = default_value;
	if (node.has_attribute(name))
		node.attribute(name).value(&result);

	return result;
}


/**
 * Read string from XML node
 */
template <Genode::size_t CAPACITY>
Genode::String<CAPACITY>
Decorator::string_attribute(Xml_node const &node, char const *attr,
                            Genode::String<CAPACITY> const &default_value)
{
	if (!node.has_attribute(attr))
		return default_value;

	char buf[CAPACITY];
	node.attribute(attr).value(buf, sizeof(buf));
	return Genode::String<CAPACITY>(Genode::Cstring(buf));
}


/**
 * Read point position from XML node
 */
static inline Decorator::Point Decorator::point_attribute(Genode::Xml_node const &point)
{
	return Point(attribute(point, "xpos", 0L),
	             attribute(point, "ypos", 0L)); }


/**
 * Read area size from XML node
 */
static inline Decorator::Area Decorator::area_attribute(Genode::Xml_node const &area)
{
	return Area(attribute(area, "width",  0UL),
	            attribute(area, "height", 0UL));
}


/**
 * Read rectangle coordinates from XML node
 */
static inline Decorator::Rect Decorator::rect_attribute(Genode::Xml_node const &rect)
{
	return Rect(point_attribute(rect), area_attribute(rect));
}


/**
 * Apply functor 'func' to all XML sub nodes of given type
 */
template <typename FUNC>
static void
Decorator::for_each_sub_node(Genode::Xml_node node, char const *type,
                             FUNC const &func)
{
	if (!node.has_sub_node(type))
		return;

	for (node = node.sub_node(type); ; node = node.next()) {

		if (node.has_type(type))
			func(node);

		if (node.last()) break;
	}
}


/**
 * Read color attribute from XML node
 */
static inline Genode::Color Decorator::color(Genode::Xml_node const &color)
{
	return attribute(color, "color", Color(0, 0, 0));
}

#endif /* _INCLUDE__DECORATOR__XML_UTILS_H_ */
