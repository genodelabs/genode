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
	bool _hovered  = false;
	bool _selected = false;

	Texture<Pixel_rgb888> const * _prev_texture = nullptr;
	Texture<Pixel_rgb888> const * _curr_texture = nullptr;

	Lazy_value<int> _blend { };

	Padding _padding { 9, 9, 2, 1 };

	Area _space() const
	{
		return Area(margin.horizontal() + _padding.horizontal(),
		            margin.vertical()   + _padding.vertical());
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

	void update(Xml_node node) override
	{
		bool const new_hovered  = _enabled(node, "hovered");
		bool const new_selected = _enabled(node, "selected");

		char const * const next_texture_name =
			new_selected ? (new_hovered ? "hselected" : "selected")
			             : (new_hovered ? "hovered"   : "default");

		Texture<Pixel_rgb888> const * next_texture =
			_factory.styles.texture(node, next_texture_name);

		if (next_texture != _curr_texture) {
			_prev_texture = _curr_texture;
			_curr_texture = next_texture;

			/* don't attempt to fade between different texture sizes */
			bool const texture_size_changed = _prev_texture && _curr_texture
			                               && _prev_texture->size() != _curr_texture->size();
			if (texture_size_changed)
				_prev_texture = nullptr;

			if (_prev_texture) {
				/*
				 * The number of blending animation steps depends on the
				 * transition. By default, for style changes, a slow animation
				 * is used. Unhovering happens a bit quicker. But when hovering
				 * or changing the selection state of a button, the transition
				 * must be quick to provide a responsive feel.
				 */
				enum { SLOW = 80, MEDIUM = 40, FAST = 3 };
				int steps = SLOW;
				if (_hovered && !new_hovered)  steps = MEDIUM;
				if (!_hovered && new_hovered)  steps = FAST;
				if (_selected != new_selected) steps = FAST;

				_blend.assign(255 << 8);
				_blend.dst(0, steps);
				animated(true);
			}
		}

		_hovered  = new_hovered;
		_selected = new_selected;

		_update_children(node);
	}

	Area min_size() const override
	{
		/* determine minimum child size */
		Area child_min_size(300, 10);
		_children.for_each([&] (Widget const &child) {
			child_min_size = child.min_size(); });

		if (!_curr_texture)
			return child_min_size;

		/* don't get smaller than the background texture */
		Area const texture_size = _curr_texture->size();

		return Area(max(_space().w() + child_min_size.w(), texture_size.w()),
		            max(_space().h() + child_min_size.h(), texture_size.h()));
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const override
	{
		static Scratch_surface scratch(_factory.alloc);

		Area const texture_size = _curr_texture ? _curr_texture->size() : Area(0, 0);
		Rect const texture_rect(Point(0, 0), texture_size);

		/*
		 * Mix from_texture and to_texture according to the blend value
		 */
		scratch.reset(texture_size);

		scratch.apply([&] (Surface<Opaque_pixel> &pixel, Surface<Additive_alpha> &alpha) {

			if (!_curr_texture)
				return;

			if (_prev_texture && animated()) {

				int const blend = _blend >> 8;

				Icon_painter::paint(pixel, texture_rect, *_curr_texture, 255);
				Icon_painter::paint(pixel, texture_rect, *_prev_texture, blend);

				Icon_painter::paint(alpha, texture_rect, *_curr_texture, 255 - blend);
				Icon_painter::paint(alpha, texture_rect, *_prev_texture, blend);
			}

			/*
			 * If no fading is possible or needed, paint only _curr_texture at
			 * full opacity.
			 */
			else {
				Icon_painter::paint(pixel, texture_rect, *_curr_texture, 255);
				Icon_painter::paint(alpha, texture_rect, *_curr_texture, 255);
			}
		});

		/*
		 * Apply blended texture to target surface
		 */
		Icon_painter::paint(pixel_surface, Rect(at, _animated_geometry.area()),
		                    scratch.texture(), 255);

		Icon_painter::paint(alpha_surface, Rect(at, _animated_geometry.area()),
		                    scratch.texture(), 255);

		if (_selected)
			at = at + Point(0, 1);

		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		_children.for_each([&] (Widget &child) {

			child.position(Point(margin.left + _padding.left,
			                     margin.top  + _padding.top));

			Area const avail = geometry().area();

			unsigned const
				w = avail.w() >= _space().w() ? avail.w() - _space().w() : 0,
				h = avail.h() >= _space().h() ? avail.h() - _space().w() : 0;

			child.size(Area(max(w, child.min_size().w()),
			                max(h, child.min_size().h())));
		});
	}


	/******************************
	 ** Animator::Item interface **
	 ******************************/

	void animate() override
	{
		_blend.animate();

		animated(_blend != _blend.dst());
	}

	private:

	/**
	 * Noncopyable
	 */
	Button_widget(Button_widget const &);
	Button_widget &operator = (Button_widget const &);

};

#endif /* _BUTTON_WIDGET_H_ */
