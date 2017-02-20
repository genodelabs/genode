/*
 * \brief  Nitpicker background
 * \author Norman Feske
 * \date   2006-06-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
		Texture_base(Area(0, 0)), Session(Genode::Session_label()),
		View(*this, View::NOT_TRANSPARENT, View::BACKGROUND, 0),
		color(25, 37, 50)
	{
		View::geometry(Rect(Point(0, 0), size));
	}


	/***********************
	 ** Session interface **
	 ***********************/

	void submit_input_event(Input::Event) override { }
	void submit_sync() override { }


	/********************
	 ** View interface **
	 ********************/

	int  frame_size(Mode const &mode) const override { return 0; }
	void frame(Canvas_base &canvas, Mode const &mode) const override { }

	void draw(Canvas_base &canvas, Mode const &mode) const override
	{
		Rect const view_rect = abs_geometry();
		Clip_guard clip_guard(canvas, view_rect);

		if (tmp_fb) {
			for (unsigned i = 0; i < 7; i++) {

				canvas.draw_box(view_rect, Color(i*2,i*6,i*16*2));
				tmp_fb->refresh(0,0,1024,768);
			}
		}

		canvas.draw_box(view_rect, color);
	}
};

#endif /* _BACKGROUND_H_ */
