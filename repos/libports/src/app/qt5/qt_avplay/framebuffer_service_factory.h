/*
 * \brief   Framebuffer service factory
 * \author  Christian Prochaska
 * \date    2016-11-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FRAMEBUFFER_SERVICE_FACTORY_H_
#define _FRAMEBUFFER_SERVICE_FACTORY_H_

/* Genode includes */
#include <base/service.h>
#include <os/slave.h>
#include <framebuffer_session/connection.h>
#include <os/single_session_service.h>
#include <nitpicker_session/connection.h>

/* Qt includes */
#include <qnitpickerplatformwindow.h>
#include <qnitpickerviewwidget/qnitpickerviewwidget.h>


struct Framebuffer_service_factory
{
	virtual Genode::Service &create(Genode::Session_state::Args const &args) = 0;

	typedef Genode::Single_session_service<Framebuffer::Session> Session_service;
};


class Nitpicker_framebuffer_service_factory : public Framebuffer_service_factory
{
	private:

		Nitpicker::Connection  _nitpicker;

		Session_service        _service;

		QNitpickerViewWidget  &_nitpicker_view_widget;
		int                    _max_width;
		int                    _max_height;

		int _limited_size(int requested_size, int max_size)
		{
			if (requested_size == 0)
				return max_size;
			else
				return (max_size > 0) ? Genode::min(requested_size, max_size) : requested_size;
		}

		static inline long _session_arg(Genode::Session_state::Args const &args, const char *key)
		{
			return Genode::Arg_string::find_arg(args.string(), key).long_value(0);
		}

	public:

		Nitpicker_framebuffer_service_factory(Genode::Env &env,
		                                      QNitpickerViewWidget &nitpicker_view_widget,
		                                      int max_width = 0,
		                                      int max_height = 0)
		: _nitpicker(env),
		  _service(_nitpicker.framebuffer_session()),
		  _nitpicker_view_widget(nitpicker_view_widget),
		  _max_width(max_width), _max_height(max_height)
		{ }

		Genode::Service &create(Genode::Session_state::Args const &args)
		{
			Framebuffer::Mode const
				mode(_limited_size(_session_arg(args, "fb_width"), _max_width),
		         	 _limited_size(_session_arg(args, "fb_height"), _max_height),
		         	 _nitpicker.mode().format());
			_nitpicker.buffer(mode, false);

			QNitpickerPlatformWindow *platform_window =
				dynamic_cast<QNitpickerPlatformWindow*>(_nitpicker_view_widget
					.window()->windowHandle()->handle());

			Nitpicker::Session::View_handle parent_view_handle =
				_nitpicker.view_handle(platform_window->view_cap());

			Nitpicker::Session::View_handle nitpicker_view_handle =
				_nitpicker.create_view(parent_view_handle);

			_nitpicker.release_view_handle(parent_view_handle);

			Framebuffer::Session_client framebuffer(_nitpicker.framebuffer_session());

			Framebuffer::Mode framebuffer_mode = framebuffer.mode();
			_nitpicker_view_widget.setNitpickerView(&_nitpicker,
			                                        nitpicker_view_handle,
			                                        0, 0,
			                                        framebuffer_mode.width(),
			                                        framebuffer_mode.height());
			return _service.service();
		}
};


class Filter_framebuffer_service_factory : public Framebuffer_service_factory
{
	private:

		typedef Genode::Slave::Connection<Framebuffer::Connection> Framebuffer_connection;

		Genode::Slave::Policy  &_policy;

		Framebuffer_connection *_slave_connection { nullptr };
		Session_service        *_service { nullptr };

	public:

		Filter_framebuffer_service_factory(Genode::Slave::Policy &policy)
		: _policy(policy)
		{ }

		~Filter_framebuffer_service_factory()
		{
			delete _service;
			delete _slave_connection;
		}

		Genode::Service &create(Genode::Session_state::Args const &args)
		{
			_slave_connection = new Framebuffer_connection(_policy, args);

			_service = new Session_service(*_slave_connection);

			return _service->service();
		}
};


#endif /* _FRAMEBUFFER_SERVICE_FACTORY_H_ */
