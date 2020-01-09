/*
 * \brief  Window layouter
 * \author Norman Feske
 * \date   2015-12-31
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _OPERATIONS_H_
#define _OPERATIONS_H_

/* Genode includes */
#include <util/interface.h>

/* local includes */
#include "window.h"

namespace Window_layouter { struct Operations; }


struct Window_layouter::Operations : Interface
{
	virtual void close(Window_id) = 0;
	virtual void toggle_fullscreen(Window_id) = 0;
	virtual void focus(Window_id) = 0;
	virtual void to_front(Window_id) = 0;
	virtual void drag(Window_id, Window::Element, Point clicked, Point curr) = 0;
	virtual void finalize_drag(Window_id, Window::Element, Point clicked, Point final) = 0;
	virtual void screen(Target::Name const &) = 0;
};

#endif /* _OPERATIONS_H_ */
