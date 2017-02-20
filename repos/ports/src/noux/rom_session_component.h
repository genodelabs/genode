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

namespace Noux {

	struct Rom_dataspace_info;
	class Rom_session_component;
}


struct Noux::Rom_dataspace_info : Dataspace_info
{
	Rom_dataspace_info(Dataspace_capability ds) : Dataspace_info(ds) { }

	~Rom_dataspace_info() { }

	Dataspace_capability fork(Ram_session        &,
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

		/**
		 * Label of ROM session requested for the binary of a forked process
		 *
		 * In this case, the loading of the binary must be omitted because the
		 * address space is replayed by the fork operation. Hence, requests for
		 * such ROM modules are answered by an invalid dataspace, which is
		 * handled in 'Child::Process'.
		 */
		static Name forked_magic_binary_name() { return "(forked)"; }

	private:

		Allocator            &_alloc;
		Rpc_entrypoint       &_ep;
		Vfs::Dir_file_system &_root_dir;
		Dataspace_registry   &_ds_registry;

		/**
		 * Dataspace obtained from the VFS
		 */
		struct Vfs_dataspace
		{
			Vfs::Dir_file_system &root_dir;

			Name                 const name;
			Dataspace_capability const ds;

			Vfs_dataspace(Vfs::Dir_file_system &root_dir, Name const &name)
			:
				root_dir(root_dir), name(name), ds(root_dir.dataspace(name.string()))
			{ }

			~Vfs_dataspace() { root_dir.release(name.string(), ds); }
		};

		Constructible<Vfs_dataspace> _rom_from_vfs;

		/**
		 * Wrapped ROM session at core
		 */
		Constructible<Rom_connection> _rom_from_parent;

		Dataspace_capability _init_ds_cap(Env &env, Name const &name)
		{
			if (name.string()[0] == '/') {
				_rom_from_vfs.construct(_root_dir, name);
				return _rom_from_vfs->ds;
			}

			if (name == forked_magic_binary_name())
				return Dataspace_capability();

			_rom_from_parent.construct(env, name.string());
			Dataspace_capability ds = _rom_from_parent->dataspace();
			return ds;
		}

		Dataspace_capability const _ds_cap;

	public:

		Rom_session_component(Allocator &alloc, Env &env, Rpc_entrypoint &ep,
		                      Vfs::Dir_file_system &root_dir,
		                      Dataspace_registry &ds_registry, Name const &name)
		:
			_alloc(alloc), _ep(ep), _root_dir(root_dir), _ds_registry(ds_registry),
			_ds_cap(_init_ds_cap(env, name))
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
