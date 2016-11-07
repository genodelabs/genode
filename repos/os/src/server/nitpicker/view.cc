/*
 * \brief  Nitpicker view implementation
 * \author Norman Feske
 * \date   2006-08-09
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/pixel_rgb565.h>

#include <nitpicker_gfx/texture_painter.h>
#include <nitpicker_gfx/box_painter.h>

#include "view.h"
#include "clip_guard.h"
#include "session.h"
#include "draw_label.h"


/***************
 ** Utilities **
 ***************/

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
	draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, BLACK);
	while (--d > 1)
		draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, color);
	draw_rect(canvas, r.x1() - d, r.y1() - d, r.w() + 2*d, r.h() + 2*d, BLACK);
}


/**********
 ** View **
 **********/

void View::title(const char *title)
{
	Genode::strncpy(_title, title, TITLE_LEN);

	/* calculate label size, the position is defined by the view stack */
	_label_rect = Rect(Point(0, 0), label_size(_session.label().string(), _title));
}


void View::frame(Canvas_base &canvas, Mode const &mode) const
{
	if (!_session.label_visible())
		return;

	Rect const geometry = abs_geometry();

	draw_frame(canvas, geometry, _session.color(), frame_size(mode));
}


/**
 * Return texture painter mode depending on nitpicker state and session policy
 */
static Texture_painter::Mode
texture_painter_mode(Mode const &mode, Session const &session)
{
	/*
	 * Tint view unless it belongs to a domain that is explicitly configured to
	 * display the raw client content or if belongs to the focused domain.
	 */
	if (session.content_client() || session.has_same_domain(mode.focused_session()))
		return Texture_painter::SOLID;

	return Texture_painter::MIXED;
}


void View::draw(Canvas_base &canvas, Mode const &mode) const
{
	Texture_painter::Mode const op = texture_painter_mode(mode, _session);

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
	if (!canvas.clip().valid() || !&_session) return;

	if (tmp_fb) {
		for (unsigned i = 0; i < 2; i++) {
			canvas.draw_box(view_rect, Color(i*8,i*24,i*16*8));
			tmp_fb->refresh(0,0,1024,768);
		}
	}

	/* allow alpha blending only if the raw client content is enabled */
	bool allow_alpha = _session.content_client();

	/* draw view content */
	Color const mix_color = Color(_session.color().r >> 1,
	                              _session.color().g >> 1,
	                              _session.color().b >> 1);

	if (_session.texture()) {
		canvas.draw_texture(_buffer_off + view_rect.p1(), *_session.texture(),
		                    op, mix_color, allow_alpha);
	} else {
		canvas.draw_box(view_rect, BLACK);
	}

	if (!_session.label_visible()) return;

	/* draw label */
	Color const frame_color = _session.color();
	draw_label(canvas, _label_rect.p1(), _session.label().string(), WHITE,
	           _title, frame_color);
}
