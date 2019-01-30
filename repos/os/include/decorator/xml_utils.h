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

	static Point point_attribute(Xml_node const &);
	static Area  area_attribute(Xml_node const &);
	static Rect  rect_attribute(Xml_node const &);
	static Color color(Xml_node const &);
}


/**
 * Read point position from XML node
 */
static inline Decorator::Point Decorator::point_attribute(Genode::Xml_node const &point)
{
	return Point(point.attribute_value("xpos", 0L),
	             point.attribute_value("ypos", 0L)); }


/**
 * Read area size from XML node
 */
static inline Decorator::Area Decorator::area_attribute(Genode::Xml_node const &area)
{
	return Area(area.attribute_value("width",  0UL),
	            area.attribute_value("height", 0UL));
}


/**
 * Read rectangle coordinates from XML node
 */
static inline Decorator::Rect Decorator::rect_attribute(Genode::Xml_node const &rect)
{
	return Rect(point_attribute(rect), area_attribute(rect));
}


/**
 * Read color attribute from XML node
 */
static inline Genode::Color Decorator::color(Genode::Xml_node const &color)
{
	return color.attribute_value("color", Color(0, 0, 0));
}

#endif /* _INCLUDE__DECORATOR__XML_UTILS_H_ */
