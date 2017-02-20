/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "window.h"

Decorator::Window_base::Hover Decorator::Window::hover(Point abs_pos) const
{
	Hover hover;

	if (!_decor_geometry().contains(abs_pos))
		return hover;

	hover.window_id = id();

	Rect const closer_geometry =
		_theme.absolute(_theme.element_geometry(Theme::ELEMENT_TYPE_CLOSER),
	                                            outer_geometry());
	if (closer_geometry.contains(abs_pos)) {
		hover.closer = true;
		return hover;
	}

	Rect const maximizer_geometry =
		_theme.absolute(_theme.element_geometry(Theme::ELEMENT_TYPE_MAXIMIZER),
	                                            outer_geometry());
	if (maximizer_geometry.contains(abs_pos)) {
		hover.maximizer = true;
		return hover;
	}

	Rect const title_geometry = _theme.absolute(_theme.title_geometry(),
	                                            outer_geometry());
	if (title_geometry.contains(abs_pos)) {
		hover.title = true;
		return hover;
	}

	int const x = abs_pos.x();
	int const y = abs_pos.y();

	Area const theme_size = _theme.background_size();

	hover.left_sizer   = x < outer_geometry().x1() + (int)theme_size.w()/2;
	hover.right_sizer  = x > outer_geometry().x2() - (int)theme_size.w()/2;
	hover.top_sizer    = y < outer_geometry().y1() + (int)theme_size.h()/2;
	hover.bottom_sizer = y > outer_geometry().y2() - (int)theme_size.h()/2;

	return hover;
}
