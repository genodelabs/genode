/*
 * \brief  Startup USB driver library
 * \author Sebastian Sumpf
 * \date   2013-02-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>

extern void start_usb_driver(Genode::Env &env);


namespace Usb_driver {

	using namespace Genode;

	struct Driver_starter { virtual void start_driver() = 0; };
	struct Main;
}


struct Usb_driver::Main : Driver_starter
{
	Env &_env;

	/*
	 * Defer the startup of the USB driver until the first configuration
	 * becomes available. This is needed in scenarios where the configuration
	 * is dynamically generated and supplied to the USB driver via the
	 * report-ROM service.
	 */
	struct Initial_config_handler
	{
		Driver_starter &_driver_starter;

		Attached_rom_dataspace _config;

		Signal_handler<Initial_config_handler> _config_handler;

		void _handle_config()
		{
			_config.update();

			if (_config.xml().type() == "config")
				_driver_starter.start_driver();
		}

		Initial_config_handler(Env &env, Driver_starter &driver_starter)
		:
			_driver_starter(driver_starter),
			_config(env, "config"),
			_config_handler(env.ep(), *this, &Initial_config_handler::_handle_config)
		{
			_config.sigh(_config_handler);
			_handle_config();
		}
	};

	void _handle_start()
	{
		if (_initial_config_handler.constructed()) {
			_initial_config_handler.destruct();
			start_usb_driver(_env);
		}
	}

	Signal_handler<Main> _start_handler {
		_env.ep(), *this, &Main::_handle_start };

	Reconstructible<Initial_config_handler> _initial_config_handler { _env, *this };

	/*
	 * Called from 'Initial_config_handler'
	 */
	void start_driver() override
	{
		Signal_transmitter(_start_handler).submit();
	}

	Main(Env &env) : _env(env) { }
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Usb_driver::Main main(env);
}
