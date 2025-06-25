/*
 * \brief  Framebuffer driver that uses a framebuffer supplied by the core rom.
 * \author Johannes Kliemann
 * \author Norman Feske
 * \date   2017-06-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_io_mem_dataspace.h>
#include <dataspace/capability.h>
#include <capture_session/connection.h>
#include <timer_session/connection.h>

namespace Framebuffer {
	using namespace Genode;
	struct Main;
}


struct Framebuffer::Main
{
	using Area  = Capture::Area;
	using Pixel = Capture::Pixel;

	Env &_env;

	Attached_rom_dataspace _platform_info { _env, "platform_info" };

	struct Info
	{
		addr_t   addr;
		Area     size;
		unsigned bpp;
		unsigned pitch;
		unsigned type;

		static Info from_platform_info(Node const &node)
		{
			Info result { };
			node.with_optional_sub_node("boot", [&] (Node const &boot) {
				boot.with_optional_sub_node("framebuffer", [&] (Node const &fb) {
					result = {
						.addr   = fb.attribute_value("phys",  0UL),
						.size = { fb.attribute_value("width",  0U),
						          fb.attribute_value("height", 0U) },
						.bpp    = fb.attribute_value("bpp",    0U),
						.pitch  = fb.attribute_value("pitch",  0U),
						.type   = fb.attribute_value("type",   0U)
					};
				});
			});
			return result;
		}

		Area phys_area() const
		{
			auto const sanitized_bpp = bpp ? : 1;
			auto const width = pitch ? pitch * 8 / sanitized_bpp : size.w;

			return { width, size.h };
		}

		enum { TYPE_RGB_COLOR = 1 };

		void print(Output &out) const
		{
			Genode::print(out, size, "x", bpp, " @ ", (void*)addr, " "
			              "type=", type, " pitch=", pitch, " -> ", phys_area());
		}
	};

	Info const _info = Info::from_platform_info(_platform_info.node());

	void _check_info() const
	{
		if (_info.bpp != 32 || _info.type != Info::TYPE_RGB_COLOR ) {
			error("unsupported resolution (bpp or/and type), platform info:\n",
			      _platform_info.node());
			throw Exception();
		}
	}

	bool const _checked_info = ( _check_info(), true );

	Attached_io_mem_dataspace _fb_ds { _env, _info.addr,
	                                   _info.pitch*_info.size.h, true };

	Capture::Connection _capture { _env };

	Capture::Connection::Screen _captured_screen { _capture, _env.rm(), {
	                                               .px       = _info.phys_area(),
	                                               .mm       = { },
	                                               .viewport = { { }, _info.size },
	                                               .rotate   = { },
	                                               .flip     = { } } };
	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	void _handle_timer()
	{
		Surface<Pixel> surface(_fb_ds.local_addr<Pixel>(), _info.phys_area());

		_captured_screen.apply_to_surface(surface);
	}

	Main(Env &env) : _env(env)
	{
		log("using boot framebuffer: ", _info);

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(10*1000);
	}
};


void Component::construct(Genode::Env &env) { static Framebuffer::Main inst(env); }
