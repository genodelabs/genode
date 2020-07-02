/*
 * \brief   Common types used within nitpicker
 * \date    2017-11-166
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/color.h>
#include <base/allocator.h>
#include <base/log.h>
#include <os/surface.h>
#include <os/pixel_rgb888.h>

namespace Gui { }

namespace Nitpicker {

	using namespace Genode;
	using namespace Gui;
	using Pixel = Pixel_rgb888;

	typedef Surface_base::Point Point;
	typedef Surface_base::Area  Area;
	typedef Surface_base::Rect  Rect;

	/*
	 * Symbolic names for some important colors
	 */
	static inline Color black() { return Color(0, 0, 0); }
	static inline Color white() { return Color(255, 255, 255); }

	class Gui_session;
	class View_stack;

	static inline Area max_area(Area a1, Area a2)
	{
		return Area(max(a1.w(), a2.w()), max(a1.h(), a2.h()));
	}
}

#endif /* _TYPES_H_ */
