/*
 * \brief  Widget that handles hovered/selected state and hosts a child widget
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BUTTON_WIDGET_H_
#define _BUTTON_WIDGET_H_

/* demo includes */
#include <scout_gfx/icon_painter.h>
#include <util/lazy_value.h>

/* local includes */
#include "widget.h"
#include "scratch_surface.h"

namespace Menu_view { struct Button_widget; }


struct Menu_view::Button_widget : Widget, Animator::Item
{
	bool hovered  = false;
	bool selected = false;

	Texture<Pixel_rgb888> const * default_texture = nullptr;
	Texture<Pixel_rgb888> const * hovered_texture = nullptr;

	Lazy_value<int> blend;

	Padding padding { 9, 9, 2, 1 };

	Area _space() const
	{
		return Area(margin.horizontal() + padding.horizontal(),
		            margin.vertical()   + padding.vertical());
	}

	static bool _enabled(Xml_node node, char const *attr)
	{
		return node.attribute_value(attr, false);
	}

	Button_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id), Animator::Item(factory.animator)
	{
		margin = { 4, 4, 4, 4 };
	}

	void update(Xml_node node)
	{
		bool const new_hovered  = _enabled(node, "hovered");
		bool const new_selected = _enabled(node, "selected");

		if (new_selected) {
			default_texture = _factory.styles.texture(node, "selected");
			hovered_texture = _factory.styles.texture(node, "hselected");
		} else {
			default_texture = _factory.styles.texture(node, "default");
			hovered_texture = _factory.styles.texture(node, "hovered");
		}

		if (new_hovered != hovered) {

			if (new_hovered) {
				blend.dst(255 << 8, 3);
			} else {
				blend.dst(0, 20);
			}
			animated(blend != blend.dst());
		}

		hovered  = new_hovered;
		selected = new_selected;

		_update_children(node);

		bool const dy = selected ? 1 : 0;

		if (Widget *child = _children.first())
			child->geometry(Rect(Point(margin.left + padding.left,
			                           margin.top  + padding.top + dy),
			                     child->min_size()));
	}

	Area min_size() const override
	{
		/* determine minimum child size */
		Widget const * const child = _children.first();
		Area const child_min_size = child ? child->min_size() : Area(300, 10);

		/* don't get smaller than the background texture */
		Area const texture_size = default_texture->size();

		return Area(max(_space().w() + child_min_size.w(), texture_size.w()),
		            max(_space().h() + child_min_size.h(), texture_size.h()));
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		static Scratch_surface<Pixel_rgb888> scratch(_factory.alloc);

		Area const texture_size = default_texture->size();
		Rect const texture_rect(Point(0, 0), texture_size);

		/*
		 * Mix from_texture and to_texture according to the blend value
		 */
		scratch.reset(texture_size);

		Surface<Pixel_rgb888> scratch_pixel_surface = scratch.pixel_surface();
		Surface<Pixel_alpha8> scratch_alpha_surface = scratch.alpha_surface();

		Icon_painter::paint(scratch_pixel_surface, texture_rect, *default_texture, 255);
		Icon_painter::paint(scratch_alpha_surface, texture_rect, *default_texture, 255);

		Icon_painter::paint(scratch_pixel_surface, texture_rect, *hovered_texture, blend >> 8);
		Icon_painter::paint(scratch_alpha_surface, texture_rect, *hovered_texture, blend >> 8);

		/*
		 * Apply blended texture to target surface
		 */
		Icon_painter::paint(pixel_surface, Rect(at, _animated_geometry.area()),
		                    scratch.texture(), 255);

		Icon_painter::paint(alpha_surface, Rect(at, _animated_geometry.area()),
		                    scratch.texture(), 255);

		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		for (Widget *w = _children.first(); w; w = w->next())
			w->size(Area(geometry().w() - _space().w(),
			             geometry().h() - _space().h()));
	}


	/******************************
	 ** Animator::Item interface **
	 ******************************/

	void animate() override
	{
		blend.animate();

		animated(blend != blend.dst());
	}
};

#endif /* _BUTTON_WIDGET_H_ */
