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

#include <os/pixel_rgb565.h>

#include <nitpicker_gfx/texture_painter.h>
#include <nitpicker_gfx/box_painter.h>

#include "view_component.h"
#include "clip_guard.h"
#include "session_component.h"
#include "draw_label.h"


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
		draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, black());
		while (--d > 1)
			draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, color);
		draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, black());
	}


	/**
	 * Return texture painter mode depending on nitpicker state and session policy
	 */
	static Texture_painter::Mode
	texture_painter_mode(Focus const &focus, View_owner const &owner)
	{
		/*
		 * Tint view unless it belongs to a domain that is explicitly configured to
		 * display the raw client content or if belongs to the focused domain.
		 */
		if (owner.content_client() || focus.same_domain_as_focused(owner))
			return Texture_painter::SOLID;

		return Texture_painter::MIXED;
	}
}

using namespace Nitpicker;


void View_component::title(Font const &font, Title const &title)
{
	_title = title;

	/* calculate label size, the position is defined by the view stack */
	_label_rect = Rect(Point(0, 0), label_size(font, _owner.label().string(),
	                                           _title.string()));
}


void View_component::frame(Canvas_base &canvas, Focus const &focus) const
{
	if (!_owner.label_visible())
		return;

	Rect const geometry = abs_geometry();

	draw_frame(canvas, geometry, _owner.color(), frame_size(focus));
}


void View_component::draw(Canvas_base &canvas, Font const &font, Focus const &focus) const
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
	if (!canvas.clip().valid() || !&_owner) return;

	/* allow alpha blending only if the raw client content is enabled */
	bool allow_alpha = _owner.content_client();

	/* draw view content */
	Color const owner_color = _owner.color();
	Color const mix_color = Color(owner_color.r >> 1,
	                              owner_color.g >> 1,
	                              owner_color.b >> 1);

	Texture_base const *texture = _owner.texture();
	if (texture) {
		canvas.draw_texture(_buffer_off + view_rect.p1(), *texture, op,
		                    mix_color, allow_alpha);
	} else {
		canvas.draw_box(view_rect, black());
	}

	if (!_owner.label_visible()) return;

	/* draw label */
	Color const frame_color = owner_color;
	draw_label(canvas, font, _label_rect.p1(), _owner.label().string(), white(),
	           _title.string(), frame_color);
}


void View_component::apply_origin_policy(View_component &pointer_origin)
{
	if (owner().origin_pointer() && !has_parent(pointer_origin))
		_assign_parent(&pointer_origin);

	if (!owner().origin_pointer() && has_parent(pointer_origin))
		_assign_parent(0);
}


bool View_component::input_response_at(Point p) const
{
	Rect const view_rect = abs_geometry();

	/* check if point lies outside view geometry */
	if ((p.x() < view_rect.x1()) || (p.x() > view_rect.x2())
	 || (p.y() < view_rect.y1()) || (p.y() > view_rect.y2()))
		return false;

	/* if view uses an alpha channel, check the input mask */
	if (_owner.content_client() && _owner.uses_alpha())
		return _owner.input_mask_at(p - view_rect.p1() - _buffer_off);

	return true;
}


int View_component::frame_size(Focus const &focus) const
{
	if (!_owner.label_visible()) return 0;

	return focus.focused(_owner) ? 5 : 3;
}


bool View_component::transparent() const
{
	return _transparent || _owner.uses_alpha();
}


bool View_component::uses_alpha() const
{
	return _owner.uses_alpha();
}
