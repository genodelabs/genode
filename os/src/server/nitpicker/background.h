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

#include "view.h"
#include "clip_guard.h"

struct Background : private Texture, Session, View
{
	Genode::Color color;

	/*
	 * The background uses no texture. Therefore
	 * we can pass a null pointer as texture argument
	 * to the Session constructor.
	 */
	Background(Canvas::Area size)
	:
		Texture(Area(0, 0)), Session(Genode::Session_label(""), 0, false),
		View(*this, View::NOT_STAY_TOP, View::NOT_TRANSPARENT,
		     View::BACKGROUND, Canvas::Rect(Canvas::Point(0, 0), size)),
		color(25, 37, 50)
	{ }


	/***********************
	 ** Session interface **
	 ***********************/

	void submit_input_event(Input::Event) { }


	/********************
	 ** View interface **
	 ********************/

	int  frame_size(Mode const &mode) const { return 0; }
	void frame(Canvas &canvas, Mode const &mode) { }

	void draw(Canvas &canvas, Mode const &mode) const
	{
		Clip_guard clip_guard(canvas, *this);
		canvas.draw_box(*this, color);
	}
};

#endif /* _BACKGROUND_H_ */
