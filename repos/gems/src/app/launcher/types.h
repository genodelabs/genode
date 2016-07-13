/*
 * \brief  Common types for launcher
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <decorator/xml_utils.h>
#include <nitpicker_session/nitpicker_session.h>

namespace Launcher {
	using namespace Genode;

	typedef String<128> Label;

	static Nitpicker::Session::Label selector(Label label)
	{
		/*
		 * Append label separator to uniquely identify the subsystem.
		 * Otherwise, the selector may be ambiguous if the label of one
		 * subsystem starts with the label of another subsystem.
		 */
		char selector[Nitpicker::Session::Label::size()];
		snprintf(selector, sizeof(selector), "%s ->", label.string());
		return Nitpicker::Session::Label(Cstring(selector));
	}

	using Decorator::area_attribute;
	using Decorator::point_attribute;

	typedef Nitpicker::Point Point;
	typedef Nitpicker::Rect  Rect;
}

#endif /* _TYPES_H_ */
