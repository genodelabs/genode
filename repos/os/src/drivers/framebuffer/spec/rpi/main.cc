/*
 * \brief  Framebuffer driver for Raspberry Pi
 * \author Norman Feske
 * \date   2013-09-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <util/reconstructible.h>
#include <capture_session/connection.h>
#include <platform_session/connection.h>
#include <blit/blit.h>
#include <timer_session/connection.h>

namespace Framebuffer {
	using namespace Genode;
	struct Main;
};


struct Framebuffer::Main
{
	using Area  = Capture::Area;
	using Pixel = Capture::Pixel;

	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Platform::Connection _platform { _env };

	Area const _size { 1024, 768 };

	Platform::Framebuffer_info _fb_info { _size.w(), _size.h(), 32 };

	bool const _fb_initialized = ( _platform.setup_framebuffer(_fb_info), true );

	Attached_io_mem_dataspace _fb_ds { _env, _fb_info.addr, _fb_info.size };

	Capture::Connection _capture { _env };

	Capture::Connection::Screen _captured_screen { _capture, _env.rm(), _size };

	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	void _handle_timer()
	{
		Area const phys_size { _fb_info.phys_width, _fb_info.phys_height };

		Surface<Pixel> surface(_fb_ds.local_addr<Pixel>(), phys_size);

		_captured_screen.apply_to_surface(surface);
	}

	Main(Genode::Env &env) : _env(env)
	{
		log("--- rpi_fb_drv started ---");

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(10*1000);
	}
};


void Component::construct(Genode::Env &env)
{
	static Framebuffer::Main main(env);
}
