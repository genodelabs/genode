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

#include <nitpicker_gfx/chunky_canvas.h>
#include "menubar.h"

template <typename PT>
class Chunky_menubar : public Chunky_texture<PT>,
	                   public Session, public Menubar
{
	private:

		Chunky_canvas<PT> _chunky_canvas;

	public:

		Chunky_menubar(PT *pixels, Area size)
		:
			Chunky_texture<PT>(pixels, 0, size), Session("", *this, 0, BLACK),
			Menubar(_chunky_canvas, size, *this), _chunky_canvas(pixels, size)
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
		void draw(Canvas const &canvas, Mode const &mode)
		{
			Clip_guard clip_guard(canvas, *this);

			/* draw menubar content */
			canvas.draw_texture(*this, BLACK, p1(), Canvas::SOLID);
		}
};

#endif
