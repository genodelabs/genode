/*
 * \brief  Tar service child
 * \author Christian Prochaska
 * \date   2010-08-26
 *
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TAR_SERVER_CHILD_H_
#define _TAR_SERVER_CHILD_H_

/* Genode includes */
#include <base/child.h>
#include <base/service.h>
#include <init/child_policy.h>

class Tar_server_child : public Genode::Child_policy,
                         public Init::Child_policy_enforce_labeling
{
	private:

		const char *_unique_name;

		/*
		 * Entry point used for serving the parent interface
		 */
		enum { STACK_SIZE = 8*1024 };
		Genode::Rpc_entrypoint _entrypoint;

		struct Resources
		{
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;
			Genode::Rm_connection  rm;

			/* session costs to be deduced from usable ram quota */
			enum { DONATIONS = Genode::Rm_connection::RAM_QUOTA +
                               Genode::Cpu_connection::RAM_QUOTA +
                               Genode::Ram_connection::RAM_QUOTA };

			Resources(char const *label, Genode::size_t ram_quota)
			: ram(label), cpu(label)
			{
				if (ram_quota > DONATIONS)
					ram_quota -= DONATIONS;
				else ram_quota = 0;

				ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(
					ram.cap(), ram_quota);
			}
		} _resources;

		Genode::Child                _child;
		Genode::Service_registry     *_parent_services;

		Genode::Lock                 _tar_server_ready_lock;
		Genode::Root_capability      _tar_server_root;

		Genode::Dataspace_capability _config_ds;

		Init::Child_policy_provide_rom_file _config_policy;
		Init::Child_policy_provide_rom_file _tar_ds_policy;

		Genode::Dataspace_capability _create_config_ds()
		{
			using namespace Genode;

			Dataspace_capability ds_cap;

			/* the tar server asked for the config file containing the name of the tar file */
			const char *config_string = "<config><archive name=\"tar_ds\"/></config>";
			size_t config_size = strlen(config_string) + 1;
			ds_cap = env()->ram_session()->alloc(config_size);
			char *config_addr = env()->rm_session()->attach(ds_cap);
			strncpy(config_addr, config_string, config_size);
			env()->rm_session()->detach(config_addr);

			return ds_cap;
		}

	public:

		enum { DONATIONS = Resources::DONATIONS };

		Tar_server_child(const char                     *unique_name,
		                 Genode::Dataspace_capability    elf_ds,
		                 Genode::size_t                  ram_quota,
		                 Genode::Cap_session            *cap_session,
		                 Genode::Service_registry       *parent_services,
		                 Genode::Dataspace_capability    tar_ds)
		: Init::Child_policy_enforce_labeling(unique_name),
		  _unique_name(unique_name),
		  _entrypoint(cap_session, STACK_SIZE, unique_name, false),
		  _resources(unique_name, ram_quota),
		  _child(elf_ds, _resources.ram.cap(), _resources.cpu.cap(),
		         _resources.rm.cap(), &_entrypoint, this),
		  _parent_services(parent_services),
		  _tar_server_ready_lock(Genode::Cancelable_lock::LOCKED),
		  _config_ds(_create_config_ds()),
		  _config_policy("config", _config_ds, &_entrypoint),
		  _tar_ds_policy("tar_ds", tar_ds, &_entrypoint)
		{
			_entrypoint.activate();
			_tar_server_ready_lock.lock();
		}

		~Tar_server_child()
		{
			using namespace Genode;
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(_config_ds));
		}

		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return _unique_name; }

		void filter_session_args(const char *, char *args, Genode::size_t args_len)
		{
			Init::Child_policy_enforce_labeling::filter_session_args(0, args, args_len);
		}

		bool announce_service(const char              *name,
		                      Genode::Root_capability  root,
		                      Genode::Allocator       *alloc)
		{
			if (Genode::strcmp(name, "ROM") != 0)
				return false;

			_tar_server_root = root;
			_tar_server_ready_lock.unlock();
			return true;
		}

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			using namespace Genode;

			PDBG("service_name = %s", service_name);

			Service *service = 0;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name, args)))
				return service;

			/* check for tar_ds file request */
			if ((service = _tar_ds_policy.resolve_session_request(service_name, args)))
				return service;

			service = _parent_services->find(service_name);
			if (!service) {
				service = new (env()->heap()) Parent_service(service_name);
				_parent_services->insert(service);
			}
			return service;
		}

		Genode::Root_capability tar_server_root() { return _tar_server_root; }
};

#endif /* _TAR_SERVER_CHILD_H_ */
