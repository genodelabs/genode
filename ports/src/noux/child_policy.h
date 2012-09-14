/*
 * \brief  Noux child policy
 * \author Norman Feske
 * \date   2012-02-25
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
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
#include <file_descriptor_registry.h>
#include <local_noux_service.h>
#include <local_rm_service.h>

namespace Noux {

	class Child_policy : public Genode::Child_policy
	{
		private:

			enum { NAME_MAX_LEN = 128 };
			char                                _name_buf[NAME_MAX_LEN];
			char                         const *_name;
			Init::Child_policy_enforce_labeling _labeling_policy;
			Init::Child_policy_provide_rom_file _binary_policy;
			Init::Child_policy_provide_rom_file _args_policy;
			Init::Child_policy_provide_rom_file _env_policy;
			Local_noux_service                 &_local_noux_service;
			Local_rm_service                   &_local_rm_service;
			Service_registry                   &_parent_services;
			Family_member                      &_family_member;
			File_descriptor_registry           &_file_descriptor_registry;
			Signal_context_capability           _exit_context_cap;
			Ram_session                        &_ref_ram_session;

		public:

			Child_policy(char               const *name,
			             Dataspace_capability      binary_ds,
			             Dataspace_capability      args_ds,
			             Dataspace_capability      env_ds,
			             Rpc_entrypoint           &entrypoint,
			             Local_noux_service       &local_noux_service,
			             Local_rm_service         &local_rm_service,
			             Service_registry         &parent_services,
			             Family_member            &family_member,
			             File_descriptor_registry &file_descriptor_registry,
			             Signal_context_capability exit_context_cap,
			             Ram_session              &ref_ram_session)
			:
				_name(strncpy(_name_buf, name, sizeof(_name_buf))),
				_labeling_policy(_name),
				_binary_policy("binary", binary_ds, &entrypoint),
				_args_policy(  "args",   args_ds,   &entrypoint),
				_env_policy(   "env",    env_ds,    &entrypoint),
				_local_noux_service(local_noux_service),
				_local_rm_service(local_rm_service),
				_parent_services(parent_services),
				_family_member(family_member),
				_file_descriptor_registry(file_descriptor_registry),
				_exit_context_cap(exit_context_cap),
				_ref_ram_session(ref_ram_session)
			{ }

			const char *name() const { return _name; }

			Service *resolve_session_request(const char *service_name,
			                                 const char *args)
			{
				Service *service = 0;

				/* check for local ROM file requests */
				if ((service =   _args_policy.resolve_session_request(service_name, args))
				 || (service =    _env_policy.resolve_session_request(service_name, args))
				 || (service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for locally implemented noux service */
				if (strcmp(service_name, Session::service_name()) == 0)
					return &_local_noux_service;

				/*
				 * Check for the creation of an RM session, which is used by
				 * the dynamic linker to manually manage a part of the address
				 * space.
				 */
				if (strcmp(service_name, Rm_session::service_name()) == 0)
					return &_local_rm_service;

				return _parent_services.find(service_name);
			}

			void filter_session_args(const char *service,
			                         char *args, size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
			}

			void exit(int exit_value)
			{
				PINF("child %s exited with exit value %d", _name, exit_value);

				/*
				 * Close all open file descriptors. This is necessary to unblock
				 * the parent if it is trying to read from a pipe (connected to
				 * the child) before calling 'wait4()'.
				 */
				_file_descriptor_registry.flush();

				_family_member.wakeup_parent(exit_value);

				/* handle exit of the init process */
				if (_family_member.parent() == 0)
					Signal_transmitter(_exit_context_cap).submit();
			}

			Ram_session *ref_ram_session()
			{
				return &_ref_ram_session;
			}
	};
}

#endif /* _NOUX__CHILD_POLICY_H_ */

