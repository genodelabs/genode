/*
 * \brief  Frame-buffer driver for the i.MX53
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \author Norman Feske
 * \date   2012-06-21
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <timer_session/connection.h>
#include <dataspace/client.h>

/* local includes */
#include <driver.h>

namespace Framebuffer {
	using namespace Genode;
	struct Main;
};


struct Framebuffer::Main
{
	using Pixel = Capture::Pixel;

	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Driver _driver { _env, _config.xml() };

	Area const _size = _driver.screen_size();

	Attached_ram_dataspace _fb_ds { _env.ram(), _env.rm(),
	                                _size.count()*sizeof(Pixel), WRITE_COMBINED };

	Capture::Connection _capture { _env };

	Capture::Connection::Screen _captured_screen { _capture, _env.rm(), _size };

	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	void _handle_timer()
	{
		Surface<Pixel> surface(_fb_ds.local_addr<Pixel>(), _size);

		_captured_screen.apply_to_surface(surface);
	}

	Main(Env &env) : _env(env)
	{
		log("--- i.MX53 framebuffer driver ---");

		if (!_driver.init(Dataspace_client(_fb_ds.cap()).phys_addr())) {
			error("could not initialize display");
			struct Could_not_initialize_display : Exception { };
			throw Could_not_initialize_display();
		}

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(10*1000);
	}
};


void Component::construct(Genode::Env &env) { static Framebuffer::Main main(env); }
