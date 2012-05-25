/*
 * \brief   Framebuffer session component
 * \author  Christian Prochaska
 * \date    2012-04-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <nitpicker_view/client.h>
#include <util/arg_string.h>
#include <util/misc_math.h>

#include "framebuffer_session_component.h"

namespace Framebuffer {


	int Session_component::_limited_size(int requested_size, int max_size)
	{
		if (requested_size == 0)
			return max_size;
		else
			return (max_size > 0) ? Genode::min(requested_size, max_size) : requested_size;
	}


	static inline long session_arg(const char *arg, const char *key)
	{
		return Genode::Arg_string::find_arg(arg, key).long_value(0);
	}


	Session_component::Session_component(const char *args,
	                                     QNitpickerViewWidget &nitpicker_view_widget,
	                                     int max_width,
	                                     int max_height)
	:	_nitpicker(Nitpicker::Connection(
		            _limited_size(session_arg(args, "fb_width"), max_width),
		            _limited_size(session_arg(args, "fb_height"), max_height))),
		_framebuffer(_nitpicker.framebuffer_session())
	{
		Nitpicker::View_capability nitpicker_view_cap = _nitpicker.create_view();
		Mode _mode = _framebuffer.mode();
		nitpicker_view_widget.setNitpickerView(nitpicker_view_cap,
		                                       0, 0,
		                                       _mode.width(),
		                                       _mode.height());
	}


	Genode::Dataspace_capability Session_component::dataspace()
	{
		return _framebuffer.dataspace();
	}


	void Session_component::release()
	{
		_framebuffer.release();
	}


	Mode Session_component::mode() const
	{
		return _framebuffer.mode();
	}


	void Session_component::mode_sigh(Genode::Signal_context_capability sigh_cap)
	{
		_framebuffer.mode_sigh(sigh_cap);
	}


	void Session_component::refresh(int x, int y, int w, int h)
	{
		_framebuffer.refresh(x, y, w, h);
	}
}
