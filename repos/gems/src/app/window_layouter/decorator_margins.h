/*
 * \brief  Decoration size information
 * \author Norman Feske
 * \date   2018-09-28
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DECORATOR_MARGINS_H_
#define _DECORATOR_MARGINS_H_

/* local includes */
#include <types.h>

namespace Window_layouter { struct Decorator_margins; }


struct Window_layouter::Decorator_margins
{
	unsigned top, bottom, left, right;

	Decorator_margins(Xml_node node)
	:
		top   (node.attribute_value("top",    0UL)),
		bottom(node.attribute_value("bottom", 0UL)),
		left  (node.attribute_value("left",   0UL)),
		right (node.attribute_value("right",  0UL))
	{ }

	/**
	 * Convert outer geometry to inner geometry
	 */
	Rect inner_geometry(Rect outer) const
	{
		return Rect(outer.p1() + Point(left,  top),
		            outer.p2() - Point(right, bottom));
	}

	/**
	 * Convert inner geometry to outer geometry
	 */
	Rect outer_geometry(Rect inner) const
	{
		return Rect(inner.p1() - Point(left,  top),
		            inner.p2() + Point(right, bottom));
	}
};

#endif /* _DECORATOR_MARGINS_H_ */
