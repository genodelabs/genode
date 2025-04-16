/*
 * \brief  Framebuffer driver front end
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2025 Genode Labs GmbH
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

	Attached_rom_dataspace _config   { _env, "config" };
	Expanding_reporter     _reporter { _env, "connectors", "connectors" };

	Area _virt_size { 1, 1 };
	Area _phys_size { 1, 1 };

	void _handle_config();
	Area _configured_size(Xml_node const &);

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

	Surface<Pixel> surface(_fb_ds->local_addr<Pixel>(), _phys_size);

	_captured_screen->apply_to_surface(surface);
}


Capture::Area Vesa_driver::Main::_configured_size(Xml_node const & config)
{
	Area area { config.attribute_value( "width", 0U),
	            config.attribute_value("height", 0U) };

	auto with_connector = [&](auto const &node) {

		bool enabled = node.attribute_value("enabled", true);

		if (!enabled || node.attribute_value("name", String<5>("none")) != "VESA")
			return;

		auto  width = node.attribute_value("width"  , 0U);
		auto height = node.attribute_value("height" , 0U);

		area = { width, height };
	};

	/* lookup config of discrete connectors */
	config.for_each_sub_node("connector", [&] (auto const &conn) {
		with_connector(conn);
	});

	/* lookup config of mirrored connectors */
	config.with_optional_sub_node("merge", [&] (auto const & merge) {
		merge.for_each_sub_node("connector", [&] (auto const & conn) {
			with_connector(conn);
		});
	});

	return area;
}


void Vesa_driver::Main::_handle_config()
{
	_config.update();

	if (!_config.valid())
		return;

	auto const & config          = _config.xml();
	auto const   period_ms       = config.attribute_value("period_ms", 20UL);
	Area const   configured_size = _configured_size(config);

	if (configured_size == _virt_size)
		return;

	_fb_ds.destruct();
	_timer.trigger_periodic(0);

	/* set VESA mode */
	auto apply_mode = [&] (auto const configure) {
		enum { BITS_PER_PIXEL = 32 };

		Area virt_size { configure.w, configure.h };
		Area phys_size = virt_size;

		if (Framebuffer::set_mode(_reporter, phys_size, virt_size, BITS_PER_PIXEL) != 0) {
			warning("could not set ", configure);
			return false;
		}

		/*
		 * Framebuffer::set_mode may return a size different from the passed
		 * argument. In particular, when passing a size of (0,0), the function
		 * sets and returns the highest screen mode possible.
		 */
		_phys_size = phys_size;
		_virt_size = virt_size;

		log("using ", _virt_size, " (", _phys_size, ")");

		return true;
	};

	if (!apply_mode(configured_size)) {
		/* in case of failure try to re-setup previous mode */
		apply_mode(_virt_size);
	}

	/* enable pixel capturing */
	_fb_ds.construct(_env.rm(), Framebuffer::hw_framebuffer());

	using Attr = Capture::Connection::Screen::Attr;

	_captured_screen.construct(_capture, _env.rm(), Attr {
		.px       = _phys_size,
		.mm       = { },
		.viewport = { { }, _virt_size },
		.rotate   = { },
		.flip     = { } });

	_timer.trigger_periodic(period_ms*1000);
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Vesa_driver::Main inst(env);
}
