/*
 * \brief  ROM session implementation used by Noux processes
 * \author Norman Feske
 * \date   2013-07-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__ROM_SESSION_COMPONENT_H_
#define _NOUX__ROM_SESSION_COMPONENT_H_

/* Genode includes */
#include <rom_session/connection.h>
#include <base/rpc_server.h>

/* Noux includes */
#include "vfs_io_channel.h"

namespace Noux {

	struct Vfs_dataspace;
	struct Rom_dataspace_info;
	class Rom_session_component;
}


/**
 * Dataspace obtained from the VFS
 */
struct Noux::Vfs_dataspace
{
	typedef Child_policy::Name Name;

	Vfs::File_system       &root_dir;
	Vfs_io_waiter_registry &vfs_io_waiter_registry;

	Name                 const name;
	Genode::Ram_session &ram;
	Genode::Region_map  &rm;
	Genode::Allocator   &alloc;

	Dataspace_capability  ds;
	bool                  got_ds_from_vfs { true };

	Vfs_dataspace(Vfs::File_system &root_dir,
	              Vfs_io_waiter_registry &vfs_io_waiter_registry,
	              Name const &name,
			      Genode::Ram_session &ram, Genode::Region_map &rm,
			      Genode::Allocator &alloc)
	:
		root_dir(root_dir), vfs_io_waiter_registry(vfs_io_waiter_registry),
		name(name), ram(ram), rm(rm), alloc(alloc)
	{
		ds = root_dir.dataspace(name.string());

		if (!ds.valid()) {

			got_ds_from_vfs = false;

			Vfs::Directory_service::Stat stat_out;

			if (root_dir.stat(name.string(), stat_out) != Vfs::Directory_service::STAT_OK)
				return;

			if (stat_out.size == 0)
				return;

			Vfs::Vfs_handle *file;
			if (root_dir.open(name.string(),
					          Vfs::Directory_service::OPEN_MODE_RDONLY,
					          &file,
					          alloc) != Vfs::Directory_service::OPEN_OK)
				return;

			Vfs_handle_context read_context;
			file->context = &read_context;

			ds = ram.alloc(stat_out.size);

			char *addr = rm.attach(static_cap_cast<Genode::Ram_dataspace>(ds));

			for (Vfs::file_size bytes_read = 0; bytes_read < stat_out.size; ) {

				Registered_no_delete<Vfs_io_waiter>
					vfs_io_waiter(vfs_io_waiter_registry);

				while (!file->fs().queue_read(file, stat_out.size - bytes_read))
					vfs_io_waiter.wait_for_io();

				Vfs::File_io_service::Read_result read_result;

				Vfs::file_size out_count;

				for (;;) {
					read_result = file->fs().complete_read(file, addr + bytes_read,
					                                       stat_out.size,
					                                       out_count);
					if (read_result != Vfs::File_io_service::READ_QUEUED)
						break;

					read_context.vfs_io_waiter.wait_for_io();
				}

				if (read_result != Vfs::File_io_service::READ_OK) {
					Genode::error("Error reading dataspace from VFS");
					rm.detach(addr);
					ram.free(static_cap_cast<Genode::Ram_dataspace>(ds));
					root_dir.close(file);
					return;
				}

				bytes_read += out_count;
				file->advance_seek(out_count);
			}

			rm.detach(addr);
			root_dir.close(file);
		}
	}

	~Vfs_dataspace()
	{
		if (got_ds_from_vfs)
			root_dir.release(name.string(), ds);
		else
			ram.free(static_cap_cast<Genode::Ram_dataspace>(ds));
	}
};


struct Noux::Rom_dataspace_info : Dataspace_info
{
	Rom_dataspace_info(Dataspace_capability ds) : Dataspace_info(ds) { }

	~Rom_dataspace_info() { }

	Dataspace_capability fork(Ram_allocator      &,
	                          Region_map         &,
	                          Allocator          &alloc,
	                          Dataspace_registry &ds_registry,
	                          Rpc_entrypoint     &) override
	{
		ds_registry.insert(new (alloc) Rom_dataspace_info(ds_cap()));
		return ds_cap();
	}

	void poke(Region_map &, addr_t dst_offset, char const *src, size_t len)
	{
		error("attempt to poke onto a ROM dataspace");
	}
};


/**
 * Local ROM service
 *
 * Depending on the ROM name, the data is provided by the VFS (if the name
 * starts with a '/' or the parent's ROM service. If the name empty, an
 * invalid dataspace capability is returned (this is used for the binary
 * ROM session of a forked process).
 */
class Noux::Rom_session_component : public Rpc_object<Rom_session>
{
	public:

		typedef Child_policy::Name Name;

	private:

		Allocator              &_alloc;
		Rpc_entrypoint         &_ep;
		Vfs::File_system       &_root_dir;
		Vfs_io_waiter_registry &_vfs_io_waiter_registry;
		Dataspace_registry     &_ds_registry;

		Constructible<Vfs_dataspace> _rom_from_vfs;

		/**
		 * Wrapped ROM session at core
		 */
		Constructible<Rom_connection> _rom_from_parent;

		Dataspace_capability _init_ds_cap(Env &env, Name const &name)
		{
			if (name.string()[0] == '/') {
				_rom_from_vfs.construct(_root_dir, _vfs_io_waiter_registry,
				                        name, env.ram(), env.rm(), _alloc);
				return _rom_from_vfs->ds;
			}

			_rom_from_parent.construct(env, name.string());
			Dataspace_capability ds = _rom_from_parent->dataspace();
			return ds;
		}

		Dataspace_capability const _ds_cap;

	public:

		Rom_session_component(Allocator &alloc, Env &env, Rpc_entrypoint &ep,
		                      Vfs::File_system &root_dir,
		                      Vfs_io_waiter_registry &vfs_io_waiter_registry,
		                      Dataspace_registry &ds_registry, Name const &name)
		:
			_alloc(alloc), _ep(ep), _root_dir(root_dir),
			_vfs_io_waiter_registry(vfs_io_waiter_registry),
			_ds_registry(ds_registry), _ds_cap(_init_ds_cap(env, name))
		{
			_ep.manage(this);
			_ds_registry.insert(new (alloc) Rom_dataspace_info(_ds_cap));
		}

		~Rom_session_component()
		{
			Rom_dataspace_info *ds_info = nullptr;

			/*
			 * Lookup and lock ds info instead of directly accessing
			 * the '_ds_info' member.
			 */
			_ds_registry.apply(_ds_cap, [&] (Rom_dataspace_info *info) {

				if (!info) {
					error("~Rom_session_component: unexpected !info");
					return;
				}

				_ds_registry.remove(info);

				info->dissolve_users();

				ds_info = info;
			});
			destroy(_alloc, ds_info);
			_ep.dissolve(this);
		}


		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace()
		{
			return static_cap_cast<Rom_dataspace>(_ds_cap);
		}

		void sigh(Signal_context_capability) { }
};

#endif /* _NOUX__ROM_SESSION_COMPONENT_H_ */
