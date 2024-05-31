/*
 * \brief  Framebuffer driver front end
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <util/reconstructible.h>
#include <timer_session/connection.h>
#include <capture_session/connection.h>
#include <blit/painter.h>
#include <os/pixel_rgb888.h>

/* local includes */
#include "framebuffer.h"

namespace Vesa_driver {
	using namespace Genode;
	struct Main;
}


struct Vesa_driver::Main
{
	using Pixel = Capture::Pixel;
	using Area  = Capture::Area;

	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };


	/*
	 * Config
	 */

	Attached_rom_dataspace _config { _env, "config" };

	Area _size { 1, 1 };

	void _handle_config();

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };


	/*
	 * Capture
	 */

	Capture::Connection _capture { _env };

	Constructible<Capture::Connection::Screen> _captured_screen { };


	/*
	 * Timer
	 */

	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	void _handle_timer();


	/*
	 * Driver
	 */

	bool const _framebuffer_initialized = ( Framebuffer::init(_env, _heap), true );

	Constructible<Attached_dataspace> _fb_ds { };

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_timer.sigh(_timer_handler);

		_handle_config();
	}
};


void Vesa_driver::Main::_handle_timer()
{
	if (!_fb_ds.constructed())
		return;

	Surface<Pixel> surface(_fb_ds->local_addr<Pixel>(), _size);

	_captured_screen->apply_to_surface(surface);
}


void Vesa_driver::Main::_handle_config()
{
	_config.update();

	Xml_node const config = _config.xml();

	Area const configured_size { config.attribute_value("width",  0U),
	                             config.attribute_value("height", 0U) };
	if (configured_size == _size)
		return;

	_size = Area { };
	_fb_ds.destruct();
	_timer.trigger_periodic(0);

	/* set VESA mode */
	{
		enum { BITS_PER_PIXEL = 32 };

		struct Pretty_mode
		{
			Area size;
			void print(Output &out) const {
				Genode::print(out, "VESA mode ", size, "@", (int)BITS_PER_PIXEL); }
		};

		unsigned width  = configured_size.w(),
		         height = configured_size.h();

		if (Framebuffer::set_mode(width, height, BITS_PER_PIXEL) != 0) {
			warning("could not set ", Pretty_mode{configured_size});
			return;
		}

		/*
		 * Framebuffer::set_mode may return a size different from the passed
		 * argument. In paricular, when passing a size of (0,0), the function
		 * sets and returns the highest screen mode possible.
		 */
		_size = Area { width, height };

		log("using ", Pretty_mode{_size});
	}

	/* enable pixel capturing */
	_fb_ds.construct(_env.rm(), Framebuffer::hw_framebuffer());
	_captured_screen.construct(_capture, _env.rm(), _size);

	unsigned long const period_ms = config.attribute_value("period_ms", 20U);
	_timer.trigger_periodic(period_ms*1000);
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Vesa_driver::Main inst(env);
}
