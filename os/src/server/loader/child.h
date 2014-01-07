/*
 * \brief  Loader child interface
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* Genode includes */
#include <base/child.h>
#include <util/arg_string.h>
#include <init/child_policy.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>


namespace Loader {

	using namespace Genode;

	class Child : public Child_policy
	{
		private:

			struct Label {
				char string[Session::Name::MAX_SIZE];
				Label(char const *l) { strncpy(string, l, sizeof(string)); }
			} _label;

			Native_pd_args _pd_args;

			Rpc_entrypoint &_ep;

			struct Resources
			{
				Ram_connection ram;
				Cpu_connection cpu;
				Rm_connection  rm;

				Resources(char const *label,
				          Ram_session_client       &ram_session_client,
				          size_t                    ram_quota,
				          Signal_context_capability fault_sigh)
				: ram(label), cpu(label)
				{
					/* deduce session costs from usable ram quota */
					size_t session_donations = Rm_connection::RAM_QUOTA +
					                           Cpu_connection::RAM_QUOTA +
					                           Ram_connection::RAM_QUOTA;

					if (ram_quota > session_donations)
						ram_quota -= session_donations;
					else ram_quota = 0;

					ram.ref_account(ram_session_client);
					ram_session_client.transfer_quota(ram.cap(), ram_quota);

					/*
					 * Install CPU exception and RM fault handler assigned by
					 * the loader client via 'Loader_session::fault_handler'.
					 */
					cpu.exception_handler(Thread_capability(), fault_sigh);
					rm.fault_handler(fault_sigh);
				}
			} _resources;


			Service_registry &_parent_services;
			Service &_local_nitpicker_service;
			Service &_local_rom_service;
			Service &_local_cpu_service;
			Service &_local_rm_service;

			Rom_session_client _binary_rom_session;

			Init::Child_policy_provide_rom_file _binary_policy;
			Init::Child_policy_enforce_labeling _labeling_policy;
			Init::Child_policy_pd_args          _pd_args_policy;

			Genode::Child _child;

			Rom_session_capability _rom_session(char const *name)
			{
				try {
					char args[Session::Name::MAX_SIZE];
					snprintf(args, sizeof(args), "ram_quota=4K, filename=\"%s\"", name);
					return static_cap_cast<Rom_session>(_local_rom_service.session(args, Affinity()));
				} catch (Genode::Parent::Service_denied) {
					PERR("Lookup for ROM module \"%s\" failed", name);
					throw;
				}
			}

		public:

			Child(char                const *binary_name,
			      char                const *label,
			      Native_pd_args      const &pd_args,
			      Rpc_entrypoint            &ep,
			      Ram_session_client        &ram_session_client,
			      size_t                     ram_quota,
			      Service_registry          &parent_services,
			      Service                   &local_rom_service,
			      Service                   &local_cpu_service,
			      Service                   &local_rm_service,
			      Service                   &local_nitpicker_service,
			      Signal_context_capability fault_sigh)
			:
				_label(label),
				_pd_args(pd_args),
				_ep(ep),
				_resources(_label.string, ram_session_client, ram_quota, fault_sigh),
				_parent_services(parent_services),
				_local_nitpicker_service(local_nitpicker_service),
				_local_rom_service(local_rom_service),
				_local_cpu_service(local_cpu_service),
				_local_rm_service(local_rm_service),
				_binary_rom_session(_rom_session(binary_name)),
				_binary_policy("binary", _binary_rom_session.dataspace(), &_ep),
				_labeling_policy(_label.string),
				_pd_args_policy(&_pd_args),
				_child(_binary_rom_session.dataspace(),
				       _resources.ram.cap(), _resources.cpu.cap(),
				       _resources.rm.cap(), &_ep, this)
			{ }

			~Child()
			{
				_local_rom_service.close(_binary_rom_session);
			}


			/****************************
			 ** Child-policy interface **
			 ****************************/

			char           const *name()    const { return _label.string; }
			Native_pd_args const *pd_args() const { return &_pd_args; }

			void filter_session_args(char const *service, char *args, size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
				_pd_args_policy. filter_session_args(service, args, args_len);
			}

			Service *resolve_session_request(const char *name,
			                                 const char *args)
			{
				Service *service = 0;

				if ((service = _binary_policy.resolve_session_request(name, args)))
					return service;

				if (!strcmp(name, "Nitpicker")) return &_local_nitpicker_service;
				if (!strcmp(name, "ROM"))       return &_local_rom_service;
				if (!strcmp(name, "CPU"))       return &_local_cpu_service;
				if (!strcmp(name, "RM"))        return &_local_rm_service;

				/* populate session-local parent service registry on demand */
				service = _parent_services.find(name);
				if (!service) {
					service = new (env()->heap()) Parent_service(name);
					_parent_services.insert(service);
				}
				return service;
			}
	};
}

#endif /* _CHILD_H_ */
