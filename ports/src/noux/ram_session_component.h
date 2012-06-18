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
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__RAM_SESSION_COMPONENT_H_
#define _NOUX__RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <ram_session/client.h>
#include <base/rpc_server.h>
#include <base/env.h>

/* Noux includes */
#include <dataspace_registry.h>

namespace Noux {

	struct Ram_dataspace_info : Dataspace_info,
	                            List<Ram_dataspace_info>::Element
	{
		Ram_dataspace_info(Ram_dataspace_capability ds_cap)
		: Dataspace_info(ds_cap) { }

		Dataspace_capability fork(Ram_session_capability ram,
		                          Dataspace_registry    &,
		                          Rpc_entrypoint        &)
		{
			size_t const size = Dataspace_client(ds_cap()).size();

			Ram_dataspace_capability dst_ds;

			try {
				dst_ds = Ram_session_client(ram).alloc(size);
			} catch (...) {
				return Dataspace_capability();
			}

			void *src = 0;
			try {
				src = env()->rm_session()->attach(ds_cap());
			} catch (...) { }

			void *dst = 0;
			try {
				dst = env()->rm_session()->attach(dst_ds);
			} catch (...) { }

			if (src && dst)
				memcpy(dst, src, size);

			if (src) env()->rm_session()->detach(src);
			if (dst) env()->rm_session()->detach(dst);

			if (!src || !dst) {
				Ram_session_client(ram).free(dst_ds);
				return Dataspace_capability();
			}

			return dst_ds;
		}

		void poke(addr_t dst_offset, void const *src, size_t len)
		{
			if ((dst_offset >= size()) || (dst_offset + len > size())) {
			 	PERR("illegal attemt to write beyond dataspace boundary");
			 	return;
			}

			char *dst = 0;
			try {
				dst = env()->rm_session()->attach(ds_cap());
			} catch (...) { }

			if (src && dst)
				memcpy(dst + dst_offset, src, len);

			if (dst) env()->rm_session()->detach(dst);
		}
	};


	class Ram_session_component : public Rpc_object<Ram_session>
	{
		private:

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
			Ram_session_component(Dataspace_registry &registry)
			: _used_quota(0), _registry(registry) { }

			/**
			 * Destructor
			 */
			~Ram_session_component()
			{
				Ram_dataspace_info *info = 0;
				while ((info = _list.first()))
					free(static_cap_cast<Ram_dataspace>(info->ds_cap()));
			}


			/***************************
			 ** Ram_session interface **
			 ***************************/

			Ram_dataspace_capability alloc(size_t size, bool cached)
			{
				Ram_dataspace_capability ds_cap =
					env()->ram_session()->alloc(size, cached);

				Ram_dataspace_info *ds_info = new (env()->heap())
				                              Ram_dataspace_info(ds_cap);

				_used_quota += ds_info->size();

				_registry.insert(ds_info);
				_list.insert(ds_info);

				return ds_cap;
			}

			void free(Ram_dataspace_capability ds_cap)
			{
				Ram_dataspace_info *ds_info =
					dynamic_cast<Ram_dataspace_info *>(_registry.lookup_info(ds_cap));

				if (!ds_info) {
					PERR("RAM free: dataspace lookup failed");
					return;
				}
				
				_registry.remove(ds_info);
				_list.remove(ds_info);
				_used_quota -= ds_info->size();

				env()->ram_session()->free(ds_cap);
				destroy(env()->heap(), ds_info);
			}

			int ref_account(Ram_session_capability) { return 0; }
			int transfer_quota(Ram_session_capability, size_t) { return 0; }
			size_t quota() { return env()->ram_session()->quota(); }
			size_t used() { return _used_quota; }
	};
}

#endif /* _NOUX__RAM_SESSION_COMPONENT_H_ */
