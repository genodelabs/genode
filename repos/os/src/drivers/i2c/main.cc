/*
 * \brief  I2C driver main
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-08
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <util/reconstructible.h>

/* Local includes */
#include "component.h"
#include "driver.h"

namespace I2c { struct Main; }


struct I2c::Main
{
	private:

		Env         &_env;
		Sliced_heap  _sliced_heap { _env.ram(), _env.rm() };

		Attached_rom_dataspace _config { _env, "config" };

		static I2c::Driver::Args _driver_args_from_config(Xml_node config)
		{
			constexpr uint16_t const default_bus_speed_khz { 400 };
			return {
				.verbose       = config.attribute_value("verbose", false),
				.bus_no        = config.attribute_value("bus_no", 0u),
				.device_name   = config.attribute_value("device_name", Device_name()),
				.bus_speed_khz = config.attribute_value("bus_speed_khz", default_bus_speed_khz)
			};
		}

		I2c::Driver _driver { _env, _driver_args_from_config(_config.xml()) };
		I2c::Root   _root { _env.ep().rpc_ep(), _sliced_heap,
		                    _driver, _config.xml() };

	public:

		Main(Env &env) : _env(env)
		{
			_env.parent().announce(env.ep().manage(_root));

			log(_driver.name(), " started");
		}
};


void Component::construct(Genode::Env &env) { static I2c::Main main(env); }
