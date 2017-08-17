/*
 * \brief  Widget that aligns/stretches a child widget within a larger parent
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FLOAT_WIDGET_H_
#define _FLOAT_WIDGET_H_

/* local includes */
#include "widget.h"

namespace Menu_view { struct Float_widget; }


struct Menu_view::Float_widget : Widget
{
	bool _north = false, _south = false, _east = false, _west = false;

	Float_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	: Widget(factory, node, unique_id) { }

	void _place_child(Widget &child)
	{
		Area const child_min = child.min_size();

		/* space around the minimal-sized child */
		int const w_space = geometry().w() - child_min.w();
		int const h_space = geometry().h() - child_min.h();

		/* stretch child size opposite attributes are specified */
		int const w = (_east  && _west)  ? geometry().w() : child_min.w();
		int const h = (_north && _south) ? geometry().h() : child_min.h();

		/* align / center child position */
		int const x = _east  ? 0 : _west  ? w_space : w_space / 2;
		int const y = _north ? 0 : _south ? h_space : h_space / 2;

		child.geometry(Rect(Point(x, y), Area(w, h)));
	}

	void update(Xml_node node) override
	{
		_update_children(node);

		_north = node.attribute_value("north", false),
		_south = node.attribute_value("south", false),
		_east  = node.attribute_value("east",  false),
		_west  = node.attribute_value("west",  false);

		if (Widget *child = _children.first())
			_place_child(*child);
	}

	Area min_size() const override
	{
		/* determine minimum child size */
		Widget const * const child = _children.first();
		return child ? child->min_size() : Area(0, 0);
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		if (Widget *child = _children.first()) {
			_place_child(*child);
			child->size(child->geometry().area());
		}
	}
};

#endif /* _FLOAT_WIDGET_H_ */
