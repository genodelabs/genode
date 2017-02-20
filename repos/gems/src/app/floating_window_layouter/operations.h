/*
 * \brief  Floating window layouter
 * \author Norman Feske
 * \date   2015-12-31
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _OPERATIONS_H_
#define _OPERATIONS_H_

/* local includes */
#include "window.h"

namespace Floating_window_layouter { struct Operations; }


struct Floating_window_layouter::Operations
{
	virtual void close(Window_id) = 0;
	virtual void toggle_fullscreen(Window_id) = 0;
	virtual void focus(Window_id) = 0;
	virtual void to_front(Window_id) = 0;
	virtual void drag(Window_id, Window::Element, Point clicked, Point curr) = 0;
	virtual void finalize_drag(Window_id, Window::Element, Point clicked, Point final) = 0;
};

#endif /* _OPERATIONS_H_ */
