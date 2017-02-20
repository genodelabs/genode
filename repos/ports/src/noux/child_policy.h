/*
 * \brief  Noux child policy
 * \author Norman Feske
 * \date   2012-02-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__CHILD_POLICY_H_
#define _NOUX__CHILD_POLICY_H_

/* Genode includes */
#include <init/child_policy.h>

/* Noux includes */
#include <family_member.h>
#include <parent_exit.h>
#include <file_descriptor_registry.h>
#include <local_rom_service.h>

namespace Noux {

	typedef Registered<Genode::Parent_service> Parent_service;
	typedef Registry<Parent_service>           Parent_services;

	typedef Local_service<Pd_session_component>  Pd_service;
	typedef Local_service<Ram_session_component> Ram_service;
	typedef Local_service<Cpu_session_component> Cpu_service;
	typedef Local_service<Rpc_object<Session> >  Noux_service;

	class Child_policy;
}


class Noux::Child_policy : public Genode::Child_policy
{
	private:

		Name                          const _name;
		Binary_name                   const _binary_name;
		Init::Child_policy_provide_rom_file _args_policy;
		Init::Child_policy_provide_rom_file _env_policy;
		Init::Child_policy_provide_rom_file _config_policy;
		Pd_service                         &_pd_service;
		Ram_service                        &_ram_service;
		Cpu_service                        &_cpu_service;
		Noux_service                       &_noux_service;
		Local_rom_service                  &_rom_service;
		Parent_services                    &_parent_services;
		Family_member                      &_family_member;
		Parent_exit                        *_parent_exit;
		File_descriptor_registry           &_file_descriptor_registry;
		Signal_context_capability           _destruct_context_cap;
		Ram_session                        &_ref_ram;
		Ram_session_capability              _ref_ram_cap;
		int                                 _exit_value;
		bool                                _verbose;

		template <typename T>
		static Genode::Service *_find_service(Genode::Registry<T> &services,
		                                      Genode::Service::Name const &name)
		{
			Genode::Service *service = nullptr;
			services.for_each([&] (T &s) {
				if (!service && (s.name() == name))
					service = &s; });
			return service;
		}

	public:

		Child_policy(Name               const &name,
		             Binary_name        const &binary_name,
		             Dataspace_capability      args_ds,
		             Dataspace_capability      env_ds,
		             Dataspace_capability      config_ds,
		             Rpc_entrypoint           &entrypoint,
		             Pd_service               &pd_service,
		             Ram_service              &ram_service,
		             Cpu_service              &cpu_service,
		             Noux_service             &noux_service,
		             Local_rom_service        &rom_service,
		             Parent_services          &parent_services,
		             Family_member            &family_member,
		             Parent_exit              *parent_exit,
		             File_descriptor_registry &file_descriptor_registry,
		             Signal_context_capability destruct_context_cap,
		             Ram_session              &ref_ram,
		             Ram_session_capability    ref_ram_cap,
		             bool                      verbose)
		:
			_name(name),
			_binary_name(binary_name),
			_args_policy(  "args",   args_ds,   &entrypoint),
			_env_policy(   "env",    env_ds,    &entrypoint),
			_config_policy("config", config_ds, &entrypoint),
			_pd_service(pd_service),   _ram_service(ram_service),
			_cpu_service(cpu_service), _noux_service(noux_service),
			_rom_service(rom_service), _parent_services(parent_services),
			_family_member(family_member),
			_parent_exit(parent_exit),
			_file_descriptor_registry(file_descriptor_registry),
			_destruct_context_cap(destruct_context_cap),
			_ref_ram(ref_ram), _ref_ram_cap(ref_ram_cap),
			_exit_value(~0),
			_verbose(verbose)
		{ }

		int exit_value() const { return _exit_value; }

		/****************************
		 ** Child policy interface **
		 ****************************/

		Name        name()        const override { return _name; }
		Binary_name binary_name() const override { return _binary_name; }

		Ram_session &ref_ram() override { return _ref_ram; }

		Ram_session_capability ref_ram_cap() const override { return _ref_ram_cap; }

		void init(Ram_session &session, Ram_session_capability cap) override
		{
			session.ref_account(_ref_ram_cap);
		}

		Service &resolve_session_request(Service::Name const &service_name,
		                                 Session_state::Args const &args) override
		{
			Session_label const label(Genode::label_from_args(args.string()));

			/* route initial ROM requests (binary and linker) to the parent */
			if (service_name == Genode::Rom_session::service_name()) {
				if (label.last_element() == binary_name()) return _rom_service;
				if (label.last_element() == linker_name()) return _rom_service;
			}

			Genode::Service *service = nullptr;

			/* check for local ROM requests */
			if ((service = _args_policy  .resolve_session_request(service_name.string(), args.string()))
			 || (service = _env_policy   .resolve_session_request(service_name.string(), args.string()))
			 || (service = _config_policy.resolve_session_request(service_name.string(), args.string())))
				return *service;

			/* check for local services */
			if (service_name == Genode::Ram_session::service_name()) return _ram_service;
			if (service_name == Genode::Cpu_session::service_name()) return _cpu_service;
			if (service_name == Genode::Rom_session::service_name()) return _rom_service;
			if (service_name == Genode::Pd_session::service_name())  return _pd_service;
			if (service_name == Noux::Session::service_name())       return _noux_service;

			/* check for parent services */
			if ((service = _find_service(_parent_services, service_name)))
				return *service;

			throw Parent::Service_denied();
		}

		void exit(int exit_value) override
		{
			_exit_value = exit_value;

			if (_verbose || (exit_value != 0))
				log("child ", _name, " exited with exit value ", exit_value);

			/*
			 * Close all open file descriptors. This is necessary to unblock
			 * the parent if it is trying to read from a pipe (connected to
			 * the child) before calling 'wait4()'.
			 */
			_file_descriptor_registry.flush();

			_family_member.exit(exit_value);

			/* notify the parent */
			if (_parent_exit)
				_parent_exit->exit_child();
			else {
				/* handle exit of the init process */
				Signal_transmitter(_destruct_context_cap).submit();
			}
		}

		Region_map *address_space(Pd_session &pd) override
		{
			return &static_cast<Pd_session_component &>(pd).address_space_region_map();
		}
};

#endif /* _NOUX__CHILD_POLICY_H_ */
