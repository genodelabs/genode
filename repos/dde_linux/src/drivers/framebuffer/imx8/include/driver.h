/*
 * \brief  i.MX8 framebuffer driver
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \author Christian Prochaska
 * \date   2015-10-16
 */

/*
 * Copyright (C) 2015-2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __DRIVER_H__
#define __DRIVER_H__

/* Genode includes */
#include <base/component.h>
#include <dataspace/capability.h>
#include <capture_session/connection.h>
#include <timer_session/connection.h>
#include <util/reconstructible.h>
#include <base/attached_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

#include <lx_emul_c.h>

namespace Framebuffer {

	using namespace Genode;
	class Driver;
}


class Framebuffer::Driver
{
	private:

		using Area  = Capture::Area;
		using Pixel = Capture::Pixel;

		struct Configuration
		{
			struct lx_c_fb_config _lx = { .height = 16,
			                              .width  = 64,
			                              .pitch  = 64,
			                              .bpp    = 4,
			                              .addr   = nullptr,
			                              .size   = 0,
			                              .lx_fb  = nullptr };
		} _lx_config;

		Env &_env;

		Attached_rom_dataspace &_config;

		Timer::Connection _timer { _env };

		Reporter _reporter { _env, "connectors" };

		Signal_context_capability _config_sigh;

		drm_display_mode * _preferred_mode(drm_connector *connector,
		                                   unsigned &brightness);

		/*
		 * Capture
		 */

		Constructible<Capture::Connection> _capture { };

		Constructible<Capture::Connection::Screen> _captured_screen { };

		Timer::Connection _capture_timer { _env };

		Signal_handler<Driver> _capture_timer_handler {
			_env.ep(), *this, &Driver::_handle_capture_timer };

		void _handle_capture_timer()
		{
			if (!_captured_screen.constructed())
				return;

			Area const phys_size { _lx_config._lx.pitch/sizeof(Pixel),
			                       _lx_config._lx.height };

			Capture::Surface<Pixel> surface((Pixel *)_lx_config._lx.addr, phys_size);

			_captured_screen->apply_to_surface(surface);
		}

		int _force_width_from_config()
		{
			return _config.xml().attribute_value<unsigned>("force_width", 0);
		}

		int _force_height_from_config()
		{
			return _config.xml().attribute_value<unsigned>("force_height", 0);
		}

	public:

		Driver(Env &env, Attached_rom_dataspace &config)
		:
			_env(env), _config(config)
		{
			_capture_timer.sigh(_capture_timer_handler);
		}

		void finish_initialization();
		void update_mode();
		void generate_report();

		/**
		 * Register signal handler used for config updates
		 *
		 * The signal handler is artificially triggered as a side effect
		 * of connector changes.
		 */
		void config_sigh(Signal_context_capability sigh)
		{
			_config_sigh = sigh;
		}

		void trigger_reconfiguration()
		{
			/*
			 * Trigger the reprocessing of the configuration following the
			 * same ontrol flow as used for external config changes.
			 */
			if (_config_sigh.valid())
				Signal_transmitter(_config_sigh).submit();
			else
				warning("config signal handler unexpectedly undefined");
		}

		void config_changed()
		{
			_config.update();

			update_mode();

			Area const size { _lx_config._lx.width, _lx_config._lx.height };


			if (_captured_screen.constructed()) {
				_capture.destruct();
				_captured_screen.destruct();
			}

			_capture.construct(_env);
			_captured_screen.construct(*_capture, _env.rm(), size);

			_capture_timer.trigger_periodic(10*1000);
		}
};

#endif /* __DRIVER_H__ */
