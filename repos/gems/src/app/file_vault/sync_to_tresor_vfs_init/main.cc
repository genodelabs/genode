/*
 * \brief  Synchronize the File Vault to the Tresor VFS initialization
 * \author Martin Stein
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <os/vfs.h>

using namespace Genode;

struct Main : Vfs::Env::User
{
	Env &env;
	Heap heap { env.ram(), env.rm() };
	Attached_rom_dataspace config_rom { env, "config" };
	Vfs::Simple_env vfs_env { env, heap, config_rom.xml().sub_node("vfs"), *this };
	Directory root { vfs_env };

	void wakeup_vfs_user() override { }

	Main(Env &env) : env(env)
	{
		{ Append_file { root, Directory::Path("/tresor/tresor/current/data") }; }
		env.parent().exit(0);
	}
};

void Component::construct(Env &env) { static Main main(env); }
