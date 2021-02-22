/*
 * \brief  i2c driver main
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

		Genode::Env                       &_env;
		Genode::Sliced_heap                _sliced_heap;
		Genode::Attached_rom_dataspace     _config { _env, "config" };
		Genode::Constructible<I2c::Root>   _root {};
		Genode::Constructible<I2c::Driver> _driver {};

	public:

		Main(Genode::Env &env)
		:
			_env { env },
			_sliced_heap(_env.ram(), _env.rm())
		{
			_config.update();
			_driver.construct(env, _config.xml());
			_root.construct(&_env.ep().rpc_ep(), &_sliced_heap, *_driver, _config.xml());

			_env.parent().announce(env.ep().manage(*_root));

			Genode::log(_driver->name(), " started");
		}

};

void Component::construct(Genode::Env &env) { static I2c::Main main(env); }
