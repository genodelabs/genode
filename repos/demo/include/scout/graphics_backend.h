/*
 * \brief   Graphics backend interface
 * \date    2013-12-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__GRAPHICS_BACKEND_H_
#define _INCLUDE__SCOUT__GRAPHICS_BACKEND_H_

#include <scout/event.h>
#include <scout/canvas.h>

namespace Scout { struct Graphics_backend; }


/*
 * We use two buffers, a foreground buffer that is displayed on screen and a
 * back buffer. While the foreground buffer must contain valid data all the
 * time, the back buffer can be used to prepare pixel data. For example,
 * drawing multiple pixel layers with alpha channel must be done in the back
 * buffer to avoid artifacts on the screen.
 */
struct Scout::Graphics_backend
{
	virtual ~Graphics_backend() { }

	virtual Canvas_base &front() = 0;

	virtual Canvas_base &back() = 0;

	virtual void copy_back_to_front(Rect rect) = 0;

	virtual void swap_back_and_front() = 0;

	/*
	 * XXX todo mode-change notification
	 */

	virtual void position(Point p) = 0;

	virtual void bring_to_front() = 0;

	virtual void view_area(Area area) = 0;
};

#endif /* _INCLUDE__SCOUT__GRAPHICS_BACKEND_H_ */
