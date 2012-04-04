/*
 * \brief  Convenience helper for running a service as child process
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-01-25
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__SLAVE_H_
#define _INCLUDE__OS__SLAVE_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/child.h>
#include <init/child_policy.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <rm_session/connection.h>
#include <os/child_policy_dynamic_rom.h>

namespace Genode {

	/**
	 * Slave-policy class
	 *
	 * This class provides a convenience policy for single-service slaves using a
	 * white list of parent services.
	 */
	class Slave_policy : public Genode::Child_policy
	{
		protected:

			/**
			 * Return white list of services the slave is permitted to use
			 *
			 * The list is terminated via a NULL pointer.
			 */
			virtual char const **_permitted_services() const = 0;

		private:

			char const                           *_label;
			Genode::Service_registry              _parent_services;
			Genode::Rpc_entrypoint               &_entrypoint;
			Genode::Rom_connection                _binary_rom;
			Init::Child_policy_enforce_labeling   _labeling_policy;
			Init::Child_policy_provide_rom_file   _binary_policy;
			Genode::Child_policy_dynamic_rom_file _config_policy;

			bool _service_permitted(const char *service_name)
			{
				for (const char **s = _permitted_services(); *s; ++s)
					if (!Genode::strcmp(service_name, *s))
						return true;

				return false;
			}

		public:

			/**
			 * Slave-policy constructor
			 *
			 * \param label       name of the program to start
			 * \param entrypoint  entrypoint used to provide local services
			 *                    such as the config ROM service
			 * \param ram         RAM session used for buffering config data
			 *
			 * If 'ram' is set to 0, no configuration can be supplied to the
			 * slave.
			 */
			Slave_policy(const char             *label,
			             Genode::Rpc_entrypoint &entrypoint,
			             Genode::Ram_session    *ram = 0)
			:
				_label(label),
				_entrypoint(entrypoint),
				_binary_rom(_label, _label),
				_labeling_policy(_label),
				_binary_policy("binary", _binary_rom.dataspace(), &_entrypoint),
				_config_policy("config", _entrypoint, ram)
			{ }

			Genode::Dataspace_capability binary() { return _binary_rom.dataspace(); }

			/**
			 * Assign new configuration to slave
			 */
			void configure(char const *config)
			{
				_config_policy.load(config, Genode::strlen(config) + 1);
			}


			/****************************
			 ** Child_policy interface **
			 ****************************/

			const char *name() const { return _label; }

			Genode::Service *resolve_session_request(const char *service_name,
			                                         const char *args)
			{
				Genode::Service *service = 0;

				/* check for binary file request */
				if ((service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for config file request */
				if ((service = _config_policy.resolve_session_request(service_name, args)))
					return service;

				if (!_service_permitted(service_name)) {
					PERR("%s: illegal session request of service \"%s\"",
					     name(), service_name);
					return 0;
				}

				/* fill parent service registry on demand */
				if (!(service = _parent_services.find(service_name))) {
					service = new (Genode::env()->heap())
					          Genode::Parent_service(service_name);
					_parent_services.insert(service);
				}

				/* return parent service */
				return service;
			}

			void filter_session_args(const char *service,
			                         char *args, Genode::size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
			}
	};


	class Slave
	{
		private:

			struct Resources
			{
				Genode::Ram_connection ram;
				Genode::Cpu_connection cpu;
				Genode::Rm_connection  rm;

				class Quota_exceeded : public Genode::Exception { };

				Resources(const char *label, Genode::size_t ram_quota)
				: ram(label), cpu(label)
				{
					/*
					 * XXX derive donated quota from information to be provided by
					 *     the used 'Connection' interfaces
					 */
					enum { DONATED_RAM_QUOTA = 128*1024 };
					if (ram_quota >  DONATED_RAM_QUOTA)
						ram_quota -= DONATED_RAM_QUOTA;
					else
						throw Quota_exceeded();
					ram.ref_account(Genode::env()->ram_session_cap());
					Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota);
				}
			};

			Resources     _resources;
			Genode::Child _child;

		public:

			Slave(Genode::Rpc_entrypoint &entrypoint,
			      Slave_policy           &slave_policy,
			      Genode::size_t          ram_quota)
			:
				_resources(slave_policy.name(), ram_quota),
				_child(slave_policy.binary(),
				       _resources.ram.cap(), _resources.cpu.cap(),
				       _resources.rm.cap(), &entrypoint, &slave_policy)
			{ }
	};
}

#endif /* _INCLUDE__OS__SLAVE_H_ */
