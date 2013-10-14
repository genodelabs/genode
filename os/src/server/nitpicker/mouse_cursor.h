/*
 * \brief  Nitpicker mouse cursor
 * \author Norman Feske
 * \date   2006-08-18
 *
 * The Nitpicker mouse cursor is implemented as a transparent
 * view that stays always in front of all other views.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MOUSE_CURSOR_H_
#define _MOUSE_CURSOR_H_

#include <nitpicker_gfx/chunky_canvas.h>

#include "view.h"
#include "session.h"

template <typename PT>
class Mouse_cursor : public Chunky_texture<PT>, public Session, public View
{
	private:

		View_stack const &_view_stack;

	public:

		/**
		 * Constructor
		 */
		Mouse_cursor(PT const *pixels, Area size, View_stack const &view_stack)
		:
			Chunky_texture<PT>(pixels, 0, size),
			Session(Genode::Session_label(""), 0, false),
			View(*this, View::STAY_TOP, View::TRANSPARENT, View::NOT_BACKGROUND,
			     Rect()),
			_view_stack(view_stack)
		{ }


		/***********************
		 ** Session interface **
		 ***********************/

		void submit_input_event(Input::Event) { }


		/********************
		 ** View interface **
		 ********************/

		/*
		 * The mouse cursor is always displayed without a surrounding frame.
		 */

		int frame_size(Mode const &mode) const { return 0; }

		void frame(Canvas &canvas, Mode const &mode) const { }

		void draw(Canvas &canvas, Mode const &mode) const
		{
			Clip_guard clip_guard(canvas, *this);

			/* draw area behind the mouse cursor */
			_view_stack.draw_rec(view_stack_next(), 0, 0, *this);

			/* draw mouse cursor */
			canvas.draw_texture(*this, BLACK, p1(), Canvas::MASKED);
		}
};

#endif
