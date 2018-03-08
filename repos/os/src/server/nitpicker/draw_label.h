/*
 * \brief  Support functions for drawing outlined labels
 * \author Norman Feske
 * \date   2006-08-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRAW_LABEL_H_
#define _DRAW_LABEL_H_

#include "canvas.h"
#include "types.h"


namespace Nitpicker {

	/*
	 * Gap between session label and view title in pixels
	 */
	enum { LABEL_GAP = 5 };

	/**
	 * Draw black outline of string
	 */
	inline void draw_string_outline(Canvas_base &canvas, Text_painter::Font const &font,
	                                Point pos, char const *s)
	{
		for (int j = -1; j <= 1; j++)
			for (int i = -1; i <= 1; i++)
				if (i || j)
					canvas.draw_text(pos + Point(i, j), font, black(), s);
	}


	/**
	 * Return bounding box of composed label displayed with the default font
	 *
	 * \param sl  session label string
	 * \param vt  view title string
	 */
	inline Area label_size(Font const &font, const char *sl, const char *vt) {
		return Area(font.string_width(sl).decimal() + LABEL_GAP +
		            font.string_width(vt).decimal() + 2,
		            font.bounding_box().h() + 2); }


	/**
	 * Draw outlined view label
	 *
	 * View labels are composed of two parts: the session label and the view title.
	 * The unforgeable session label is defined on session creation by system
	 * policy. In contrast, the view title can individually be defined by the
	 * application.
	 */
	static inline void draw_label(Canvas_base &canvas, Font const &font, Point pos,
	                              char const *session_label, Color session_label_color,
	                              char const *view_title,    Color view_title_color)
	{
		pos = pos + Point(1, 1);

		draw_string_outline(canvas, font, pos, session_label);
		canvas.draw_text(pos, font, session_label_color, session_label);

		pos = pos + Point(font.string_width(session_label).decimal() + LABEL_GAP, 0);

		draw_string_outline(canvas, font, pos, view_title);
		canvas.draw_text(pos, font, view_title_color, view_title);
	}
}

#endif /* _DRAW_LABEL_H_ */
