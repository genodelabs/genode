/*
 * \brief  RAM service used by Noux processes
 * \author Norman Feske
 * \date   2012-02-22
 *
 * The custom implementation of the RAM session interface provides a pool of
 * RAM shared by Noux and all Noux processes. The use of a shared pool
 * alleviates the need to assign RAM quota to individual Noux processes.
 *
 * Furthermore, the custom implementation is needed to get hold of the RAM
 * dataspaces allocated by each Noux process. When forking a process, the
 * acquired information (in the form of 'Ram_dataspace_info' objects) is used
 * to create a shadow copy of the forking address space.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__RAM_SESSION_COMPONENT_H_
#define _NOUX__RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <ram_session/client.h>
#include <base/rpc_server.h>

/* Noux includes */
#include <dataspace_registry.h>

namespace Noux {
	struct Ram_dataspace_info;
	struct Ram_session_component;
	using namespace Genode;
}


struct Noux::Ram_dataspace_info : Dataspace_info,
                                  List<Ram_dataspace_info>::Element
{
	Ram_dataspace_info(Ram_dataspace_capability ds_cap)
	: Dataspace_info(ds_cap) { }

	Dataspace_capability fork(Ram_session        &ram,
	                          Region_map         &local_rm,
	                          Allocator          &alloc,
	                          Dataspace_registry &ds_registry,
	                          Rpc_entrypoint     &) override
	{
		size_t const size = Dataspace_client(ds_cap()).size();
		Ram_dataspace_capability dst_ds_cap;

		try {
			dst_ds_cap = ram.alloc(size);

			Attached_dataspace src_ds(local_rm, ds_cap());
			Attached_dataspace dst_ds(local_rm, dst_ds_cap);
			memcpy(dst_ds.local_addr<char>(), src_ds.local_addr<char>(), size);

			ds_registry.insert(new (alloc) Ram_dataspace_info(dst_ds_cap));
			return dst_ds_cap;

		} catch (...) {
			error("fork of RAM dataspace failed");

			if (dst_ds_cap.valid())
				ram.free(dst_ds_cap);

			return Dataspace_capability();
		}
	}

	void poke(Region_map &rm, addr_t dst_offset, char const *src, size_t len) override
	{
		if (!src) return;

		if ((dst_offset >= size()) || (dst_offset + len > size())) {
			error("illegal attemt to write beyond dataspace boundary");
			return;
		}

		try {
			Attached_dataspace ds(rm, ds_cap());
			memcpy(ds.local_addr<char>() + dst_offset, src, len);
		} catch (...) { warning("poke: failed to attach RAM dataspace"); }
	}
};


class Noux::Ram_session_component : public Rpc_object<Ram_session>
{
	private:

		Ram_session &_ram;

		Allocator &_alloc;

		Rpc_entrypoint &_ep;

		List<Ram_dataspace_info> _list;

		/*
		 * Track the RAM resources accumulated via RAM session allocations.
		 *
		 * XXX not used yet
		 */
		size_t _used_quota;

		Dataspace_registry &_registry;

	public:

		/**
		 * Constructor
		 */
		Ram_session_component(Ram_session &ram, Allocator &alloc,
		                      Rpc_entrypoint &ep, Dataspace_registry &registry)
		:
			_ram(ram), _alloc(alloc), _ep(ep), _used_quota(0),
			_registry(registry)
		{
			_ep.manage(this);
		}

		/**
		 * Destructor
		 */
		~Ram_session_component()
		{
			_ep.dissolve(this);
			Ram_dataspace_info *info = 0;
			while ((info = _list.first()))
				free(static_cap_cast<Ram_dataspace>(info->ds_cap()));
		}


		/***************************
		 ** Ram_session interface **
		 ***************************/

		Ram_dataspace_capability alloc(size_t size, Cache_attribute cached)
		{
			Ram_dataspace_capability ds_cap =
				_ram.alloc(size, cached);

			Ram_dataspace_info *ds_info = new (_alloc) Ram_dataspace_info(ds_cap);

			_used_quota += ds_info->size();

			_registry.insert(ds_info);
			_list.insert(ds_info);

			return ds_cap;
		}

		void free(Ram_dataspace_capability ds_cap)
		{
			Ram_dataspace_info *ds_info;

			auto lambda = [&] (Ram_dataspace_info *rdi) {
				ds_info = rdi;

				if (!ds_info) {
					error("RAM free: dataspace lookup failed");
					return;
				}

				_registry.remove(ds_info);

				ds_info->dissolve_users();

				_list.remove(ds_info);
				_used_quota -= ds_info->size();

				_ram.free(ds_cap);
			};
			_registry.apply(ds_cap, lambda);
			destroy(_alloc, ds_info);
		}

		int ref_account(Ram_session_capability) { return 0; }
		int transfer_quota(Ram_session_capability, size_t) { return 0; }
		size_t quota() { return _ram.quota(); }
		size_t used() { return _used_quota; }
};

#endif /* _NOUX__RAM_SESSION_COMPONENT_H_ */
