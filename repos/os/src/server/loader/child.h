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
#include <pd_session/connection.h>
#include <region_map/client.h>


namespace Loader {

	using namespace Genode;

	class Child : public Child_policy
	{
		private:

			typedef String<Session::Name::MAX_SIZE> Label;

			Label _label;

			Rpc_entrypoint &_ep;

			struct Resources
			{
				Pd_connection  pd;
				Ram_connection ram;
				Cpu_connection cpu;

				Resources(char const *label,
				          Ram_session_client       &ram_session_client,
				          size_t                    ram_quota,
				          Signal_context_capability fault_sigh)
				: pd(label), ram(label), cpu(label)
				{
					/* deduce session costs from usable ram quota */
					size_t session_donations = Cpu_connection::RAM_QUOTA +
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
					cpu.exception_sigh(fault_sigh);
					Region_map_client address_space(pd.address_space());
					address_space.fault_handler(fault_sigh);
				}
			} _resources;

			Genode::Child::Initial_thread _initial_thread { _resources.cpu,
			                                                _resources.pd,
			                                                _label.string() };

			Region_map_client _address_space { _resources.pd.address_space() };

			Service_registry &_parent_services;
			Service &_local_nitpicker_service;
			Service &_local_rom_service;
			Service &_local_cpu_service;
			Service &_local_pd_service;

			Rom_session_client _binary_rom_session;

			Init::Child_policy_provide_rom_file _binary_policy;
			Init::Child_policy_enforce_labeling _labeling_policy;

			Genode::Child _child;

			Rom_session_capability _rom_session(char const *name)
			{
				try {
					char args[Session::Name::MAX_SIZE];
					snprintf(args, sizeof(args), "ram_quota=4K, label=\"%s\"", name);
					return static_cap_cast<Rom_session>(_local_rom_service.session(args, Affinity()));
				} catch (Genode::Parent::Service_denied) {
					Genode::error("Lookup for ROM module \"", name, "\" failed");
					throw;
				}
			}

		public:

			Child(char                const *binary_name,
			      char                const *label,
			      Dataspace_capability       ldso_ds,
			      Rpc_entrypoint            &ep,
			      Ram_session_client        &ram_session_client,
			      size_t                     ram_quota,
			      Service_registry          &parent_services,
			      Service                   &local_rom_service,
			      Service                   &local_cpu_service,
			      Service                   &local_pd_service,
			      Service                   &local_nitpicker_service,
			      Signal_context_capability fault_sigh)
			:
				_label(label),
				_ep(ep),
				_resources(_label.string(), ram_session_client, ram_quota, fault_sigh),
				_parent_services(parent_services),
				_local_nitpicker_service(local_nitpicker_service),
				_local_rom_service(local_rom_service),
				_local_cpu_service(local_cpu_service),
				_local_pd_service(local_pd_service),
				_binary_rom_session(_rom_session(binary_name)),
				_binary_policy("binary", _binary_rom_session.dataspace(), &_ep),
				_labeling_policy(_label.string()),
				_child(_binary_rom_session.dataspace(), ldso_ds,
				       _resources.pd,  _resources.pd,
				       _resources.ram, _resources.ram,
				       _resources.cpu, _initial_thread,
				       *env()->rm_session(), _address_space, _ep, *this)
			{ }

			~Child()
			{
				_local_rom_service.close(_binary_rom_session);
			}


			/****************************
			 ** Child-policy interface **
			 ****************************/

			char const *name() const override { return _label.string(); }

			void filter_session_args(char const *service, char *args, size_t args_len) override
			{
				_labeling_policy.filter_session_args(service, args, args_len);
			}

			Service *resolve_session_request(const char *name,
			                                 const char *args) override
			{
				Service *service = 0;

				if ((service = _binary_policy.resolve_session_request(name, args)))
					return service;

				if (!strcmp(name, "Nitpicker")) return &_local_nitpicker_service;
				if (!strcmp(name, "ROM"))       return &_local_rom_service;
				if (!strcmp(name, "CPU"))       return &_local_cpu_service;
				if (!strcmp(name, "PD"))        return &_local_pd_service;

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
