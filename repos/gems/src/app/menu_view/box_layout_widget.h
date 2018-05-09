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

	bool _vertical() const { return _direction == VERTICAL; }

	unsigned _count = 0;

	/**
	 * Stack and count children, and update min_size for the whole compound
	 *
	 * This method performs the part of the layout calculation that can be
	 * done without knowing the final size of the box layout.
	 */
	void _stack_and_count_child_widgets()
	{
		/* determine largest size among our children */
		unsigned largest_size = 0;
		_children.for_each([&] (Widget const &w) {
			largest_size =
				max(largest_size, _direction == VERTICAL ? w.min_size().w()
				                                         : w.min_size().h()); });

		/* position children on one row/column */
		Point position(0, 0);
		_count = 0;
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
			_count++;
		});

		_min_size = (_direction == VERTICAL)
		          ? Area(largest_size, position.y())
		          : Area(position.x(), largest_size);
	}

	/**
	 * Adjust layout to actual size of the entire box layout widget
	 */
	void _stretch_child_widgets_to_available_size()
	{
		using Genode::max;
		unsigned const unused_pixels =
			_vertical() ? max(_geometry.h(), _min_size.h()) - _min_size.h()
			            : max(_geometry.w(), _min_size.w()) - _min_size.w();

		/* number of excess pixels at the end of the stack (fixpoint) */
		unsigned const step_fp = (_count > 0) ? (unused_pixels << 8) / _count : 0;

		unsigned consumed_fp = 0;
		_children.for_each([&] (Widget &w) {

			unsigned const next_consumed_fp = consumed_fp + step_fp;
			unsigned const padding_pixels   = (next_consumed_fp >> 8)
			                                - (consumed_fp      >> 8);
			if (_direction == VERTICAL) {
				w.position(w.geometry().p1() + Point(0, consumed_fp >> 8));
				w.size(Area(geometry().w(), w.min_size().h() + padding_pixels));
			} else {
				w.position(w.geometry().p1() + Point(consumed_fp >> 8, 0));
				w.size(Area(w.min_size().w() + padding_pixels, geometry().h()));
			}
			consumed_fp = next_consumed_fp;
		});
	}

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

		_stack_and_count_child_widgets();

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
		_stack_and_count_child_widgets();
		_stretch_child_widgets_to_available_size();
	}
};

#endif /* _BOX_LAYOUT_WIDGET_H_ */
