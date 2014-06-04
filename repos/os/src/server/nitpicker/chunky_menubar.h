/*
 * \brief  Chunky-pixel-based menubar
 * \author Norman Feske
 * \date   2006-08-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CHUNKY_MENUBAR_
#define _CHUNKY_MENUBAR_

#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/texture_painter.h>

#include "menubar.h"


template <typename PT>
class Chunky_menubar : public Texture<PT>,
                       public Session,
                       public Menubar,
                       public View
{
	private:

		Canvas<PT> _canvas;

	public:

		Chunky_menubar(PT *pixels, Area size)
		:
			Texture<PT>(pixels, 0, size),
			Session(Genode::Session_label(""), 0, false),
			View(*this, View::STAY_TOP, View::NOT_TRANSPARENT,
			     View::NOT_BACKGROUND, 0),
			_canvas(pixels, size)
		{
			View::geometry(Rect(Point(0, 0), size));
			Session::texture(this, false);
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
			Clip_guard clip_guard(canvas, geometry());

			/* draw menubar content */
			canvas.draw_texture(abs_position(), *this, Texture_painter::SOLID, BLACK, false);
		}


		/***********************
		 ** Menubar interface **
		 ***********************/

		void state(Menubar_state const state) override
		{
			*static_cast<Menubar_state *>(this) = state;

			/* choose base color dependent on the Nitpicker state */
			int r = (mode.kill()) ? 200 : (mode.xray()) ? session_color.r : (session_color.r + 100) >> 1;
			int g = (mode.kill()) ?  70 : (mode.xray()) ? session_color.g : (session_color.g + 100) >> 1;
			int b = (mode.kill()) ?  70 : (mode.xray()) ? session_color.b : (session_color.b + 100) >> 1;

			Rect const view_rect = abs_geometry();

			/* highlight first line with slightly brighter color */
			_canvas.draw_box(Rect(Point(0, 0), Area(view_rect.w(), 1)),
			                 Color(r + (r / 2), g + (g / 2), b + (b / 2)));

			/* draw slightly shaded background */
			for (unsigned i = 1; i < view_rect.h() - 1; i++) {
				r -= r > 3 ? 4 : 0;
				g -= g > 3 ? 4 : 0;
				b -= b > 4 ? 4 : 0;
				_canvas.draw_box(Rect(Point(0, i), Area(view_rect.w(), 1)), Color(r, g, b));
			}

			/* draw last line darker */
			_canvas.draw_box(Rect(Point(0, view_rect.h() - 1), Area(view_rect.w(), 1)),
			                 Color(r / 4, g / 4, b / 4));

			/* draw label */
			draw_label(_canvas, view_rect.center(label_size(session_label.string(), "")),
			           session_label.string(), WHITE, "", session_color);
		}

		using Menubar::state;
};

#endif
