/*
 * \brief  Small utility for truncating a given file
 * \author Martin Stein
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/vfs.h>

using namespace Genode;

struct Main
{
	Env &env;
	Heap heap { env.ram(), env.rm() };
	Attached_rom_dataspace config { env, "config" };
	Root_directory vfs { env, heap, config.xml().sub_node("vfs") };
	Vfs::File_system &fs { vfs.root_dir() };
	Directory::Path path { config.xml().attribute_value("path", Directory::Path { }) };
	Number_of_bytes size { config.xml().attribute_value("size", Number_of_bytes { }) };

	Main(Env &env) : env(env)
	{
		unsigned mode = Vfs::Directory_service::OPEN_MODE_WRONLY;
		Vfs::Directory_service::Stat stat { };
		if (fs.stat(path.string(), stat) != Vfs::Directory_service::STAT_OK)
			mode |= Vfs::Directory_service::OPEN_MODE_CREATE;

		Vfs::Vfs_handle *handle_ptr = nullptr;
		auto res = fs.open(path.string(), mode, &handle_ptr, heap);
		if (res != Vfs::Directory_service::OPEN_OK || (handle_ptr == nullptr)) {
			error("failed to create file '", path, "'");
			env.parent().exit(-1);
		}
		handle_ptr->fs().ftruncate(handle_ptr, size);
		handle_ptr->ds().close(handle_ptr);
		env.parent().exit(0);
	}
};

void Component::construct(Env &env) { static Main main(env); }
