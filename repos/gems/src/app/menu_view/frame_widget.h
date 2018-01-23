/*
 * \brief  Widget that hosts a child widget in a visual frame
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FRAME_WIDGET_H_
#define _FRAME_WIDGET_H_

/* local includes */
#include "widget.h"

namespace Menu_view { struct Frame_widget; }

struct Menu_view::Frame_widget : Widget
{
	Texture<Pixel_rgb888> const * texture = nullptr;

	Padding padding {  2,  2, 2, 2 };

	Area _space() const
	{
		return Area(margin.horizontal() + padding.horizontal(),
		            margin.vertical()   + padding.vertical());
	}

	Frame_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id)
	{
		margin = { 4, 4, 4, 4 };
	}

	void update(Xml_node node) override
	{
		texture = _factory.styles.texture(node, "background");

		_update_children(node);

		/*
		 * layout
		 */
		_children.for_each([&] (Widget &child) {
			child.geometry(Rect(Point(margin.left + padding.left,
			                          margin.top  + padding.top),
			                    child.min_size())); });
	}

	Area min_size() const override
	{
		/* determine minimum child size (there is only one child) */
		Area child_min_size(0, 0);
		_children.for_each([&] (Widget const &child) {
			child_min_size = child.min_size(); });

		/* don't get smaller than the background texture */
		Area const texture_size = texture ? texture->size() : Area(0, 0);

		return Area(max(_space().w() + child_min_size.w(), texture_size.w()),
		            max(_space().h() + child_min_size.h(), texture_size.h()));
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		Icon_painter::paint(pixel_surface, Rect(at, _animated_geometry.area()),
		                    *texture, 255);

		Icon_painter::paint(alpha_surface, Rect(at, _animated_geometry.area()),
		                    *texture, 255);

		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		_children.for_each([&] (Widget &child) {
			child.size(Area(geometry().w() - _space().w(),
			                geometry().h() - _space().h())); });
	}
};

#endif /* _FRAME_WIDGET_H_ */
