/*
 * \brief  Nitpicker background
 * \author Norman Feske
 * \date   2006-06-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BACKGROUND_H_
#define _BACKGROUND_H_

#include <nitpicker_gfx/box_painter.h>

#include "view.h"
#include "clip_guard.h"

struct Background : private Texture_base, Session, View
{
	Color color;

	/*
	 * The background uses no texture. Therefore
	 * we can pass a null pointer as texture argument
	 * to the Session constructor.
	 */
	Background(Area size)
	:
		Texture_base(Area(0, 0)), Session(Genode::Session_label(""), 0, false),
		View(*this, View::NOT_STAY_TOP, View::NOT_TRANSPARENT, View::BACKGROUND, 0),
		color(25, 37, 50)
	{
		View::geometry(Rect(Point(0, 0), size));
	}


	/***********************
	 ** Session interface **
	 ***********************/

	void submit_input_event(Input::Event) { }


	/********************
	 ** View interface **
	 ********************/

	int  frame_size(Mode const &mode) const { return 0; }
	void frame(Canvas_base &canvas, Mode const &mode) { }

	void draw(Canvas_base &canvas, Mode const &mode) const
	{
		Rect const view_rect = abs_geometry();
		Clip_guard clip_guard(canvas, view_rect);
		canvas.draw_box(view_rect, color);
	}
};

#endif /* _BACKGROUND_H_ */
