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
#include <widget.h>

namespace Menu_view { struct Root_widget; }


struct Menu_view::Root_widget : Widget
{
	Root_widget(Name const &name, Unique_id const &id,
	            Widget_factory &factory, Xml_node const &node)
	:
		Widget(name, id, factory, node)
	{ }

	Area animated_size() const
	{
		Area result(1, 1);

		_children.for_each([&] (Widget const &child) {
			result = child.animated_geometry().area(); });

		return result;
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
	}

	Area min_size() const override
	{
		Area result(1, 1);

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
			child.position(Point(0, 0));
			child.size(_geometry.area());
		});
	}
};

#endif /* _ROOT_WIDGET_H_ */
