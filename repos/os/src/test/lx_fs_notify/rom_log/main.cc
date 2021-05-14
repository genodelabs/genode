/*
 * \brief  Test component for the watch feature of the `lx_fs` server.
 * \author Pirmin Duss
 * \date   2020-06-17
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <base/component.h>
#include <base/attached_rom_dataspace.h>


namespace Test_lx_fs_notify
{
	using namespace Genode;

	class Main;
};


class Test_lx_fs_notify::Main
{
	private:

		Env&                   _env;
		Signal_handler<Main>   _update_handler { _env.ep(), *this, &Main::_update };
		Attached_rom_dataspace _test_rom       { _env, "outfile.txt" };

		void _update()
		{
			_test_rom.update();
			log("updated ROM content: size=", strlen(_test_rom.local_addr<const char>()));
		}

	public:

		Main(Env& env) : _env { env }
		{
			_test_rom.sigh(_update_handler);
			log("wait for ROM update");
		}
};


void Component::construct(Genode::Env& env)
{
	static Test_lx_fs_notify::Main main { env };
}
