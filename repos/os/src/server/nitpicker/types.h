/*
 * \brief   Common types used within nitpicker
 * \date    2017-11-166
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/xml_generator.h>
#include <util/color.h>
#include <base/allocator.h>
#include <gui_session/gui_session.h>

namespace Nitpicker {

	using namespace Gui;

	using Pixel = Pixel_rgb888;

	using Point = Gui::Point;
	using Area  = Gui::Area;
	using Rect  = Gui::Rect;

	static constexpr Color white() { return Color::rgb(255, 255, 255); }

	class Gui_session;
	class View_stack;

	static inline Area max_area(Area a1, Area a2)
	{
		return Area(max(a1.w, a2.w), max(a1.h, a2.h));
	}

	struct Nowhere { };

	using Pointer = Attempt<Point, Nowhere>;

	static inline void gen_attr(Xml_generator &xml, Point const point)
	{
		if (point.x) xml.attribute("xpos", point.x);
		if (point.y) xml.attribute("ypos", point.y);
	}

	static inline void gen_attr(Xml_generator &xml, Rect const rect)
	{
		gen_attr(xml, rect.at);
		if (rect.w()) xml.attribute("width",  rect.w());
		if (rect.h()) xml.attribute("height", rect.h());
	}
}

#endif /* _TYPES_H_ */
