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
		int const x = _west  ? 0 : _east  ? w_space : w_space / 2;
		int const y = _north ? 0 : _south ? h_space : h_space / 2;

		child.position(Point(x, y));
		child.size(Area(w, h));
	}

	void update(Xml_node node) override
	{
		_update_children(node);

		_north = node.attribute_value("north", false),
		_south = node.attribute_value("south", false),
		_east  = node.attribute_value("east",  false),
		_west  = node.attribute_value("west",  false);
	}

	Area min_size() const override
	{
		Area result(0, 0);

		/* determine minimum child size */
		_children.for_each([&] (Widget const &child) {
			result = child.min_size(); });

		return result;
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const override
	{
		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		_children.for_each([&] (Widget &child) {
			_place_child(child);
			child.size(child.geometry().area());
		});
	}

	/*
	 * A float widget cannot be hovered on its own. It only responds to
	 * hovering if its child is hovered. This way, multiple floats can
	 * be stacked in one frame without interfering with each other.
	 */

	Hovered hovered(Point at) const override
	{
		Hovered const result = Widget::hovered(at);

		/* respond positively whenever one of our children is hovered */
		if (result.unique_id != _unique_id)
			return result;

		return { .unique_id = { }, .detail = { } };
	}

	void gen_hover_model(Xml_generator &xml, Point at) const override
	{
		/* omit ourself from hover model unless one of our children is hovered */
		if (!_inner_geometry().contains(at))
			return;

		Widget::gen_hover_model(xml, at);
	}
};

#endif /* _FLOAT_WIDGET_H_ */
