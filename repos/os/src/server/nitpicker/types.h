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

namespace Nitpicker {

	using namespace Genode;

	typedef Surface_base::Point Point;
	typedef Surface_base::Area  Area;
	typedef Surface_base::Rect  Rect;

	/*
	 * Symbolic names for some important colors
	 */
	static inline Color black() { return Color(0, 0, 0); }
	static inline Color white() { return Color(255, 255, 255); }

	class Session_component;
	class View_stack;
}

#endif /* _TYPES_H_ */
