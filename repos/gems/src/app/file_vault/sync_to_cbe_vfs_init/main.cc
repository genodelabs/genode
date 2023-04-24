/*
 * \brief  Helps synchronizing the CBE manager to the CBE-driver initialization
 * \author Martin Stein
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <os/vfs.h>

using namespace Genode;

struct Main : private Vfs::Env::User
{
	Env                    &_env;
	Heap                    _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace  _config_rom { _env, "config" };

	Vfs::Simple_env _vfs_env { _env, _heap,
		_config_rom.xml().sub_node("vfs"), *this };

	Directory _root_dir { _vfs_env };

	void wakeup_vfs_user() override { }

	Main(Env &env) : _env { env }
	{
		{
			Append_file { _root_dir,
			              Directory::Path("/cbe/cbe/current/data") };
		}
		_env.parent().exit(0);
	}
};


void Component::construct(Env &env)
{
	static Main main(env);
}
