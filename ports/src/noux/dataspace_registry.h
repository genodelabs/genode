/*
 * \brief  Registry for dataspaces used by noux processes
 * \author Norman Feske
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__DATASPACE_REGISTRY_H_
#define _NOUX__DATASPACE_REGISTRY_H_

/* Genode includes */
#include <base/rpc_server.h>

namespace Noux {

	class Dataspace_registry;


	class Dataspace_info : public Object_pool<Dataspace_info>::Entry
	{
		private:

			size_t               _size;
			Dataspace_capability _ds_cap;

		public:

			Dataspace_info(Dataspace_capability ds_cap)
			:
				Object_pool<Dataspace_info>::Entry(ds_cap),
				_size(Dataspace_client(ds_cap).size()),
				_ds_cap(ds_cap)
			{ }

			virtual ~Dataspace_info() { }

			size_t                 size() const { return _size; }
			Dataspace_capability ds_cap() const { return _ds_cap; }

			/**
			 * Create shadow copy of dataspace
			 *
			 * \param ds_registry  registry for keeping track of
			 *                     the new dataspace
			 * \param ep           entrypoint used to serve the RPC
			 *                     interface of the new dataspace
			 *                     (used if the dataspace is a sub
			 *                     RM session)
			 * \return             capability for the new dataspace
			 */
			virtual Dataspace_capability fork(Ram_session_capability ram,
			                                  Dataspace_registry    &ds_registry,
			                                  Rpc_entrypoint        &ep) = 0;

			/**
			 * Write raw byte sequence into dataspace
			 *
			 * \param dst_offset  destination offset within dataspace
			 * \param src         data source buffer
			 * \param len         length of source buffer in bytes
			 */
			virtual void poke(addr_t dst_offset, void const *src, size_t len) = 0;
	};


	class Dataspace_registry
	{
		private:

			Object_pool<Dataspace_info> _pool;

		public:

			~Dataspace_registry()
			{
				/*
				 * At the time the destructor is called, most 'Dataspace_info'
				 * objects are expected to be gone already because
				 * 'Child::_resources' and 'Child::_child' are destructed
				 * before the 'Child::_ds_registry'. However, RM dataspaces
				 * created via 'Rm_dataspace_info::fork', are not handled by
				 * those destructors. So we have to clean them up here.
				 */
				for (;;) {
					Dataspace_info *info = _pool.first();
					if (!info)
						return;

					_pool.remove(info);
					destroy(env()->heap(), info);
				}
			}

			void insert(Dataspace_info *info)
			{
				_pool.insert(info);
			}

			void remove(Dataspace_info *info)
			{
				_pool.remove(info);
			}

			Dataspace_info *lookup_info(Dataspace_capability ds_cap)
			{
				return _pool.obj_by_cap(ds_cap);
			}
	};
}

#endif /* _NOUX__DATASPACE_REGISTRY_H_ */
