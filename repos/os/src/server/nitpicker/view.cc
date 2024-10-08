/*
 * \brief  Nitpicker view implementation
 * \author Norman Feske
 * \date   2006-08-09
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <nitpicker_gfx/texture_painter.h>
#include <nitpicker_gfx/box_painter.h>

#include <view.h>
#include <clip_guard.h>
#include <gui_session.h>
#include <draw_label.h>


/***************
 ** Utilities **
 ***************/

namespace Nitpicker {

	/**
	 * Draw rectangle
	 */
	static void draw_rect(Canvas_base &canvas, int x, int y, int w, int h,
	                      Color color)
	{
		canvas.draw_box(Rect(Point(x, y),         Area(w, 1)), color);
		canvas.draw_box(Rect(Point(x, y),         Area(1, h)), color);
		canvas.draw_box(Rect(Point(x + w - 1, y), Area(1, h)), color);
		canvas.draw_box(Rect(Point(x, y + h - 1), Area(w, 1)), color);
	}


	/**
	 * Draw outlined frame with black outline color
	 */
	static void draw_frame(Canvas_base &canvas, Rect r, Color color,
	                       int frame_size)
	{
		/* draw frame around the view */
		int d = frame_size;
		draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, Color::black());
		while (--d > 1)
			draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, color);
		draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, Color::black());
	}


	/**
	 * Return texture painter mode depending on nitpicker state and session policy
	 */
	static Texture_painter::Mode
	texture_painter_mode(Focus const &focus, View_owner const &owner)
	{
		/*
		 * Tint view unless it belongs to a domain that is explicitly
		 * configured to display the raw client content or if belongs to the
		 * focused domain.
		 */
		if (owner.content_client() || focus.same_domain_as_focused(owner))
			return Texture_painter::SOLID;

		return Texture_painter::MIXED;
	}
}


void Nitpicker::View::title(Font const &font, Title const &title)
{
	_title = title;

	/* calculate label size, the position is defined by the view stack */
	_label_rect = Rect(Point(0, 0), label_size(font, _owner.label().string(),
	                                           _title.string()));
}


void Nitpicker::View::frame(Canvas_base &canvas, Focus const &focus) const
{
	if (!_owner.label_visible())
		return;

	Rect const geometry = abs_geometry();

	draw_frame(canvas, geometry, _owner.color(), frame_size(focus));
}


void Nitpicker::View::draw(Canvas_base &canvas, Font const &font, Focus const &focus) const
{
	Texture_painter::Mode const op = texture_painter_mode(focus, _owner);

	Rect const view_rect = abs_geometry();

	/*
	 * The view content and label should never overdraw the
	 * frame of the view in non-flat Nitpicker modes. The frame
	 * is located outside the view area. By shrinking the
	 * clipping area to the view area, we protect the frame.
	 */
	Clip_guard clip_guard(canvas, view_rect);

	/*
	 * If the clipping area shrinked to zero, we do not process drawing
	 * operations.
	 */
	if (!canvas.clip().valid())
		return;

	/* allow alpha blending only if the raw client content is enabled */
	bool allow_alpha = _owner.content_client();

	/* draw view content */
	Color const owner_color = _owner.color();
	Color const mix_color = Color::rgb(owner_color.r >> 1,
	                                   owner_color.g >> 1,
	                                   owner_color.b >> 1);

	auto for_each_tile_pos = [&] (auto const &fn)
	{
		int const view_w = view_rect.w(),
		          view_h = view_rect.h();

		int const texture_w = int(_texture.size().w),
		          texture_h = int(_texture.size().h);

		if (!texture_w || !texture_h)
			return;

		int off_x = (_buffer_off.x - _texture.panning.x) % texture_w;
		int off_y = (_buffer_off.y - _texture.panning.y) % texture_h;

		if (off_x > 0) off_x -= texture_w;
		if (off_y > 0) off_y -= texture_h;

		for (int y = off_y; y < view_h; y += texture_h)
			for (int x = off_x; x < view_w; x += texture_w)
				fn(Point { x, y });
	};

	_texture.with_texture([&] (Texture_base const &texture) {
		for_each_tile_pos([&] (Point const pos) {
			canvas.draw_texture(view_rect.p1() + pos, texture, op,
			                    mix_color, allow_alpha); }); });

	if (!_texture.valid())
		canvas.draw_box(view_rect, Color::black());

	if (!_owner.label_visible()) return;

	/* draw label */
	Color const frame_color = owner_color;
	draw_label(canvas, font, _label_rect.p1(), _owner.label().string(), white(),
	           _title.string(), frame_color);
}


void Nitpicker::View::apply_origin_policy(View &pointer_origin)
{
	if (owner().origin_pointer() && !has_parent(pointer_origin))
		_assign_parent(&pointer_origin);

	if (!owner().origin_pointer() && has_parent(pointer_origin))
		_assign_parent(0);
}


bool Nitpicker::View::input_response_at(Point const p) const
{
	Rect const view_rect = abs_geometry();

	/* check if point lies outside view geometry */
	if ((p.x < view_rect.x1()) || (p.x > view_rect.x2())
	 || (p.y < view_rect.y1()) || (p.y > view_rect.y2()))
		return false;

	/* if view uses an alpha channel, check the input mask */
	if (_owner.content_client() && _owner.uses_alpha())
		return _owner.input_mask_at(p - view_rect.p1() - _buffer_off + _texture.panning);

	return true;
}


int Nitpicker::View::frame_size(Focus const &focus) const
{
	if (!_owner.label_visible()) return 0;

	return focus.focused(_owner) ? 5 : 3;
}


bool Nitpicker::View::transparent() const
{
	return _transparent || _owner.uses_alpha();
}


bool Nitpicker::View::uses_alpha() const
{
	return _owner.uses_alpha();
}
