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

#include <nitpicker_gfx/texture_painter.h>

#include "view.h"
#include "session.h"

template <typename PT>
class Mouse_cursor : public Texture<PT>,
                     public Session, public View
{
	private:

		View_stack const &_view_stack;

	public:

		/**
		 * Constructor
		 */
		Mouse_cursor(PT const *pixels, Area size, View_stack const &view_stack)
		:
			Texture<PT>(pixels, 0, size),
			Session(Genode::Session_label(""), 0, false),
			View(*this, View::STAY_TOP, View::TRANSPARENT, View::NOT_BACKGROUND, 0),
			_view_stack(view_stack)
		{ }


		/***********************
		 ** Session interface **
		 ***********************/

		void submit_input_event(Input::Event) override { }
		void submit_sync() override { }


		/********************
		 ** View interface **
		 ********************/

		/*
		 * The mouse cursor is always displayed without a surrounding frame.
		 */

		int frame_size(Mode const &mode) const override { return 0; }

		void frame(Canvas_base &canvas, Mode const &mode) const override { }

		void draw(Canvas_base &canvas, Mode const &mode) const override
		{
			Rect const view_rect = abs_geometry();

			Clip_guard clip_guard(canvas, view_rect);

			/* draw area behind the mouse cursor */
			_view_stack.draw_rec(canvas, view_stack_next(), view_rect);

			/* draw mouse cursor */
			canvas.draw_texture(view_rect.p1(), *this, Texture_painter::MASKED, BLACK, true);
		}
};

#endif
