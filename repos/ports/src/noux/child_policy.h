/*
 * \brief  Noux child policy
 * \author Norman Feske
 * \date   2012-02-25
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__CHILD_POLICY_H_
#define _NOUX__CHILD_POLICY_H_

/* Genode includes */
#include <init/child_policy.h>

/* Noux includes */
#include <family_member.h>
#include <parent_exit.h>
#include <file_descriptor_registry.h>
#include <local_noux_service.h>
#include <local_rom_service.h>

namespace Noux {

	class Child_policy : public Genode::Child_policy
	{
		private:

			char                         const *_name;
			Init::Child_policy_enforce_labeling _labeling_policy;
			Init::Child_policy_provide_rom_file _binary_policy;
			Init::Child_policy_provide_rom_file _args_policy;
			Init::Child_policy_provide_rom_file _env_policy;
			Init::Child_policy_provide_rom_file _config_policy;
			Local_noux_service                 &_local_noux_service;
			Local_rom_service                  &_local_rom_service;
			Service_registry                   &_parent_services;
			Family_member                      &_family_member;
			Parent_exit                        *_parent_exit;
			File_descriptor_registry           &_file_descriptor_registry;
			Signal_context_capability           _destruct_context_cap;
			Ram_session                        &_ref_ram_session;
			int                                 _exit_value;
			bool                                _verbose;

		public:

			Child_policy(char               const *name,
			             Dataspace_capability      binary_ds,
			             Dataspace_capability      args_ds,
			             Dataspace_capability      env_ds,
			             Dataspace_capability      config_ds,
			             Rpc_entrypoint           &entrypoint,
			             Local_noux_service       &local_noux_service,
			             Local_rom_service        &local_rom_service,
			             Service_registry         &parent_services,
			             Family_member            &family_member,
			             Parent_exit              *parent_exit,
			             File_descriptor_registry &file_descriptor_registry,
			             Signal_context_capability destruct_context_cap,
			             Ram_session              &ref_ram_session,
			             bool                      verbose)
			:
				_name(name),
				_labeling_policy(_name),
				_binary_policy("binary", binary_ds, &entrypoint),
				_args_policy(  "args",   args_ds,   &entrypoint),
				_env_policy(   "env",    env_ds,    &entrypoint),
				_config_policy("config", config_ds, &entrypoint),
				_local_noux_service(local_noux_service),
				_local_rom_service(local_rom_service),
				_parent_services(parent_services),
				_family_member(family_member),
				_parent_exit(parent_exit),
				_file_descriptor_registry(file_descriptor_registry),
				_destruct_context_cap(destruct_context_cap),
				_ref_ram_session(ref_ram_session),
				_exit_value(~0),
				_verbose(verbose)
			{ }

			int exit_value() const { return _exit_value; }

			/****************************
			 ** Child policy interface **
			 ****************************/

			const char *name() const { return _name; }

			Service *resolve_session_request(const char *service_name,
			                                 const char *args)
			{
				Service *service = 0;

				/* check for local ROM file requests */
				if ((service = _args_policy.resolve_session_request(service_name, args))
				 || (service = _env_policy.resolve_session_request(service_name, args))
				 || (service = _config_policy.resolve_session_request(service_name, args))
				 || (service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for locally implemented noux service */
				if (strcmp(service_name, Session::service_name()) == 0)
					return &_local_noux_service;

				/*
				 * Check for local ROM service
				 */
				if (strcmp(service_name, Rom_session::service_name()) == 0)
					return &_local_rom_service;

				return _parent_services.find(service_name);
			}

			void filter_session_args(const char *service,
			                         char *args, size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
			}

			void exit(int exit_value)
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

			Ram_session *ref_ram_session()
			{
				return &_ref_ram_session;
			}
	};
}

#endif /* _NOUX__CHILD_POLICY_H_ */

