/*
 * \brief   Framebuffer session component
 * \author  Christian Prochaska
 * \date    2012-04-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <nitpicker_session/client.h>
#include <util/arg_string.h>
#include <util/misc_math.h>

#include "framebuffer_session_component.h"
#include <qnitpickerplatformwindow.h>

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
	:
		_framebuffer(_nitpicker.framebuffer_session())
	{
		Framebuffer::Mode const
			mode(_limited_size(session_arg(args, "fb_width"), max_width),
		         _limited_size(session_arg(args, "fb_height"), max_height),
		         _nitpicker.mode().format());
		_nitpicker.buffer(mode, false);

		QNitpickerPlatformWindow *platform_window =
			dynamic_cast<QNitpickerPlatformWindow*>(nitpicker_view_widget
				.window()->windowHandle()->handle());

		Nitpicker::Session::View_handle parent_view_handle =
			_nitpicker.view_handle(platform_window->view_cap());

		Nitpicker::Session::View_handle nitpicker_view_handle =
			_nitpicker.create_view(parent_view_handle);

		_nitpicker.release_view_handle(parent_view_handle);

		Mode _mode = _framebuffer.mode();
		nitpicker_view_widget.setNitpickerView(&_nitpicker,
		                                       nitpicker_view_handle,
		                                       0, 0,
		                                       _mode.width(),
		                                       _mode.height());
	}


	Genode::Dataspace_capability Session_component::dataspace()
	{
		return _framebuffer.dataspace();
	}


	Mode Session_component::mode() const
	{
		return _framebuffer.mode();
	}


	void Session_component::mode_sigh(Genode::Signal_context_capability sigh_cap)
	{
		_framebuffer.mode_sigh(sigh_cap);
	}


	void Session_component::sync_sigh(Genode::Signal_context_capability sigh_cap)
	{
		_framebuffer.sync_sigh(sigh_cap);
	}

	void Session_component::refresh(int x, int y, int w, int h)
	{
		_framebuffer.refresh(x, y, w, h);
	}
}
