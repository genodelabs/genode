/*
 * \brief  Root of the widget tree
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ROOT_WIDGET_H_
#define _ROOT_WIDGET_H_

/* local includes */
#include "widget.h"

namespace Menu_view { struct Root_widget; }


struct Menu_view::Root_widget : Widget
{
	Root_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id)
	{ }

	Area animated_size() const
	{
		if (Widget const * const child = _children.first())
			return child->animated_geometry().area();

		return Area(1, 1);
	}

	void update(Xml_node node) override
	{
		char const *dialog_tag = "dialog";

		if (!node.has_type(dialog_tag)) {
			Genode::error("no valid <dialog> tag found");
			return;
		}

		if (!node.num_sub_nodes()) {
			Genode::warning("empty <dialog> node");
			return;
		}

		_update_children(node);

		if (Widget *child = _children.first())
			child->geometry(Rect(Point(0, 0), child->min_size()));
	}

	Area min_size() const override
	{
		if (Widget const * const child = _children.first())
			return child->min_size();

		return Area(1, 1);
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		if (Widget *child = _children.first())  {
			child->size(geometry().area());
			child->position(Point(0, 0));
		}
	}
};

#endif /* _ROOT_WIDGET_H_ */
