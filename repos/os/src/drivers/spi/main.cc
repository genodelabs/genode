/*
 * \brief  Spi driver main
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-04-14
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

/* Local includes */
#include "spi_driver.h"
#include "component.h"

namespace Spi {

	struct Main;

}


struct Spi::Main
{
	private:

		Genode::Env &_env;
		Genode::Heap _heap { _env.ram(), _env.rm() };

		Genode::Attached_rom_dataspace _config { _env, "config" };

		Spi::Driver *_driver { Spi::initialize(_env, _config.xml()) };
		Spi::Root    _root   { _env, _heap, *_driver, _config.xml() };

	public:

		Main(Genode::Env &env)
		:
			_env { env }
		{
			_env.parent().announce(_env.ep().manage(_root));
			Genode::log(_driver->name(), " initialized.");
		}

		explicit Main(const Main&) = delete;
		const Main& operator=(const Main&) = delete;

};

void Component::construct(Genode::Env &env) { static Spi::Main main(env); }
