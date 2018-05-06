/*
 * \brief  Widget that hosts child widgets in a row or column
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BOX_LAYOUT_WIDGET_H_
#define _BOX_LAYOUT_WIDGET_H_

/* local includes */
#include "widget.h"

namespace Menu_view { struct Box_layout_widget; }


struct Menu_view::Box_layout_widget : Widget
{
	Area _min_size; /* value cached from layout computation */

	enum Direction { VERTICAL, HORIZONTAL };

	Direction const _direction;

	Box_layout_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id),
		       _direction(node.has_type("vbox") ? VERTICAL : HORIZONTAL)
	{ }

	void update(Xml_node node) override
	{
		_update_children(node);

		/*
		 * Apply layout to the children
		 */

		/* determine largest size among our children */
		unsigned largest_size = 0;
		_children.for_each([&] (Widget const &w) {
			largest_size =
				max(largest_size, _direction == VERTICAL ? w.min_size().w()
				                                         : w.min_size().h()); });

		/* position children on one row/column */
		Point position(0, 0);
		_children.for_each([&] (Widget &w) {

			Area const child_min_size = w.min_size();

			w.position(position);

			if (_direction == VERTICAL) {
				unsigned const next_top_margin = w.next() ? w.next()->margin.top : 0;
				unsigned const dy = child_min_size.h() - min(w.margin.bottom, next_top_margin);
				position = position + Point(0, dy);
			} else {
				unsigned const next_left_margin = w.next() ? w.next()->margin.left : 0;
				unsigned const dx = child_min_size.w() - min(w.margin.right, next_left_margin);
				position = position + Point(dx, 0);
			}
		});

		_min_size = (_direction == VERTICAL)
		          ? Area(largest_size, position.y())
		          : Area(position.x(), largest_size);
	}

	Area min_size() const override
	{
		return _min_size;
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		_children.for_each([&] (Widget &w) {
			if (_direction == VERTICAL)
				w.size(Area(geometry().w(), w.min_size().h()));
			else
				w.size(Area(w.min_size().w(), geometry().h()));
		});
	}
};

#endif /* _BOX_LAYOUT_WIDGET_H_ */
