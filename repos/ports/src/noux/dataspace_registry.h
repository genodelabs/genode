/*
 * \brief  Registry for dataspaces used by noux processes
 * \author Norman Feske
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__DATASPACE_REGISTRY_H_
#define _NOUX__DATASPACE_REGISTRY_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <dataspace/client.h>

namespace Noux {
	class Dataspace_user;
	class Dataspace_info;
	class Dataspace_registry;

	struct Static_dataspace_info;

	using namespace Genode;
}


struct Noux::Dataspace_user : List<Dataspace_user>::Element
{
	virtual void dissolve(Dataspace_info &ds) = 0;
};


class Noux::Dataspace_info : public Object_pool<Dataspace_info>::Entry
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
			_size(ds_cap.valid() ? Dataspace_client(ds_cap).size() : 0),
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
		 * \param ram          backing store used for copied dataspaces
		 * \param local_rm     region map used for temporarily attaching
		 *                     dataspaces to the local address space
		 * \param alloc        allocator used for creatng new 'Dataspace_info'
		 *                     objects
		 * \param ds_registry  registry for keeping track of
		 *                     the new dataspace
		 * \param ep           entrypoint used to serve the RPC
		 *                     interface of the new dataspace
		 *                     (used if the dataspace is a sub
		 *                     RM session)
		 * \return             capability for the new dataspace
		 */
		virtual Dataspace_capability fork(Ram_session        &ram,
		                                  Region_map         &local_rm,
		                                  Allocator          &alloc,
		                                  Dataspace_registry &ds_registry,
		                                  Rpc_entrypoint     &ep) = 0;

		/**
		 * Write raw byte sequence into dataspace
		 *
		 * \param local_rm    region map used for temporarily attaching
		 *                    the targeted dataspace to the local address
		 *                    space
		 * \param dst_offset  destination offset within dataspace
		 * \param src         data source buffer
		 * \param len         length of source buffer in bytes
		 */
		virtual void poke(Region_map &local_rm, addr_t dst_offset,
		                  char const *src, size_t len) = 0;

		/**
		 * Return leaf region map that covers a given address
		 *
		 * \param addr  address that is covered by the requested region map
		 */
		virtual Capability<Region_map> lookup_region_map(addr_t const addr)
		{
			/* by default a dataspace is no sub region map, so return invalid */
			return Capability<Region_map>();
		}
};


class Noux::Dataspace_registry : public Object_pool<Dataspace_info>
{
	private:

		Allocator &_alloc;

	public:

		Dataspace_registry(Allocator &alloc) : _alloc(alloc) { }

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
			remove_all([&] (Dataspace_info *info) { destroy(_alloc, info); });
		}
};


struct Noux::Static_dataspace_info : Dataspace_info
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
		auto lambda = [this] (Static_dataspace_info *info) {

			if (!info) {
				error("lookup of binary ds info failed");
				return;
			}

			_ds_registry.remove(info);

			info->dissolve_users();
		};
		_ds_registry.apply(ds_cap(), lambda);
	}

	Dataspace_capability fork(Ram_session        &,
	                          Region_map         &,
	                          Allocator          &,
	                          Dataspace_registry &,
	                          Rpc_entrypoint     &) override
	{
		return ds_cap();
	}

	void poke(Region_map &, addr_t, char const *, size_t) override
	{
		error("attempt to poke onto a static dataspace");
	}
};

#endif /* _NOUX__DATASPACE_REGISTRY_H_ */
