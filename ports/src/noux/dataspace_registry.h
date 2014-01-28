/*
 * \brief  Registry for dataspaces used by noux processes
 * \author Norman Feske
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
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
	class Dataspace_info;


	struct Dataspace_user : List<Dataspace_user>::Element
	{
		virtual void dissolve(Dataspace_info &ds) = 0;
	};


	class Dataspace_info : public Object_pool<Dataspace_info>::Entry
	{
		private:

			size_t               _size;
			Dataspace_capability _ds_cap;
			Lock                 _users_lock;
			List<Dataspace_user> _users;

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

			void register_user(Dataspace_user &user)
			{
				Lock::Guard guard(_users_lock);
				_users.insert(&user);
			}

			void unregister_user(Dataspace_user &user)
			{
				Lock::Guard guard(_users_lock);
				_users.remove(&user);
			}

			void dissolve_users()
			{
				for (;;) {
					Dataspace_user *user = 0;
					{
						Lock::Guard guard(_users_lock);
						user = _users.first();
						if (!user)
							break;

						_users.remove(user);
					}
					user->dissolve(*this);
				}
			}

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

			/**
			 * Return leaf RM session that covers a given address
			 *
			 * \param addr  address that is covered by the requested RM session
			 */
			virtual Rm_session_capability lookup_rm_session(addr_t const addr)
			{
				/* by default a dataspace is no sub RM, so return invalid */
				return Rm_session_capability();
			}
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
				while(Dataspace_info *info = _pool.first()) {
					_pool.remove_locked(info);
					destroy(env()->heap(), info);
				}
			}

			void insert(Dataspace_info *info)
			{
				_pool.insert(info);
			}

			void remove(Dataspace_info *info)
			{
				_pool.remove_locked(info);
			}

			Dataspace_info *lookup_info(Dataspace_capability ds_cap)
			{
				return _pool.lookup_and_lock(ds_cap);
			}
	};


	struct Static_dataspace_info : Dataspace_info
	{
		Dataspace_registry &_ds_registry;

		Static_dataspace_info(Dataspace_registry &ds_registry,
		                      Dataspace_capability ds)
		: Dataspace_info(ds), _ds_registry(ds_registry)
		{
			_ds_registry.insert(this);
		}

		~Static_dataspace_info()
		{
			Static_dataspace_info *info =
				dynamic_cast<Static_dataspace_info *>(_ds_registry.lookup_info(ds_cap()));

			if (!info) {
				PERR("lookup of binary ds info failed");
				return;
			}

			_ds_registry.remove(info);

			info->dissolve_users();

		}

		Dataspace_capability fork(Ram_session_capability,
		                          Dataspace_registry &,
		                          Rpc_entrypoint &)
		{
			return ds_cap();
		}

		void poke(addr_t dst_offset, void const *src, size_t len)
		{
			PERR("Attempt to poke onto a static dataspace");
		}
	};
}

#endif /* _NOUX__DATASPACE_REGISTRY_H_ */
