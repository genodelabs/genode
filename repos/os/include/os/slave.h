/*
 * \brief  Convenience helper for running a service as child process
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-01-25
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
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
#include <pd_session/connection.h>
#include <os/child_policy_dynamic_rom.h>

namespace Genode {

	class Slave_policy;
	class Slave;
}


/**
 * Slave-policy class
 *
 * This class provides a convenience policy for single-service slaves using a
 * white list of parent services.
 */
class Genode::Slave_policy : public Genode::Child_policy
{
	protected:

		/**
		 * Return white list of services the slave is permitted to use
		 *
		 * The list is terminated via a null pointer.
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
		             Genode::Ram_session    *ram = 0,
		             const char             *binary = nullptr)
		:
			_label(label),
			_entrypoint(entrypoint),
			_binary_rom(binary ? prefixed_label(Session_label(label),
			                                    Session_label(binary)).string() : label),
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

		void configure(char const *config, size_t len)
		{
			_config_policy.load(config, len);
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
				error(name(), ": illegal session request of service \"", service_name, "\"");
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


class Genode::Slave
{
	private:

		struct Resources
		{
			Genode::Pd_connection  pd;
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;

			class Quota_exceeded : public Genode::Exception { };

			Resources(const char *label, Genode::size_t ram_quota,
			          Ram_session_capability ram_ref_cap)
			: pd(label), ram(label), cpu(label)
			{
				ram.ref_account(ram_ref_cap);
				Ram_session_client ram_ref(ram_ref_cap);

				if (ram_ref.transfer_quota(ram.cap(), ram_quota))
					throw Quota_exceeded();
			}
		} _resources;

		Genode::Child::Initial_thread _initial_thread;

		Genode::Region_map_client _address_space { _resources.pd.address_space() };
		Genode::Child             _child;

	public:

		Slave(Genode::Rpc_entrypoint &entrypoint,
		      Slave_policy           &slave_policy,
		      Genode::size_t          ram_quota,
		      Ram_session_capability  ram_ref_cap = env()->ram_session_cap(),
		      Dataspace_capability    ldso_ds = Dataspace_capability())
		:
			_resources(slave_policy.name(), ram_quota, ram_ref_cap),
			_initial_thread(_resources.cpu, _resources.pd, slave_policy.name()),
			_child(slave_policy.binary(), ldso_ds, _resources.pd, _resources.pd,
			       _resources.ram, _resources.ram, _resources.cpu, _initial_thread,
			       *env()->rm_session(), _address_space, entrypoint, slave_policy)
		{ }

		Genode::Ram_connection &ram() { return _resources.ram; }


		/***************************************
		 ** Wrappers of the 'Child' interface **
		 ***************************************/

		void yield(Genode::Parent::Resource_args const &args) {
			_child.yield(args); }

		void notify_resource_avail() const {
			_child.notify_resource_avail(); }
};

#endif /* _INCLUDE__OS__SLAVE_H_ */
