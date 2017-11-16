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

#include "session_component.h"
#include "clip_guard.h"

namespace Nitpicker { struct Background; }


struct Nitpicker::Background : private Texture_base, View_component
{
	static Color default_color() { return Color(25, 37, 50); }

	Color color = default_color();

	/*
	 * The background uses no texture. Therefore
	 * we can pass a null pointer as texture argument
	 * to the Session constructor.
	 */
	Background(View_owner &owner, Area size)
	:
		Texture_base(Area(0, 0)),
		View_component(owner, View_component::NOT_TRANSPARENT,
		                      View_component::BACKGROUND, 0)
	{
		View_component::geometry(Rect(Point(0, 0), size));
	}


	/******************************
	 ** View_component interface **
	 ******************************/

	int  frame_size(Focus const &) const override { return 0; }
	void frame(Canvas_base &canvas, Focus const &) const override { }

	void draw(Canvas_base &canvas, Focus const &) const override
	{
		Rect const view_rect = abs_geometry();
		Clip_guard clip_guard(canvas, view_rect);

		canvas.draw_box(view_rect, color);
	}
};

#endif /* _BACKGROUND_H_ */
