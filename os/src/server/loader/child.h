/*
 * \brief  Loader child interface
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
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

			Rpc_entrypoint &_ep;

			struct Resources
			{
				Ram_connection ram;
				Cpu_connection cpu;
				Rm_connection  rm;

				Resources(char const *label,
				          Ram_session_client &ram_session_client,
				          size_t ram_quota)
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
				}
			} _resources;


			Service_registry &_parent_services;
			Service &_local_nitpicker_service;
			Service &_local_rom_service;

			Rom_session_client _binary_rom_session;

			Init::Child_policy_provide_rom_file _binary_policy;
			Init::Child_policy_enforce_labeling _labeling_policy;

			int _max_width, _max_height;

			Genode::Child _child;

			Rom_session_capability _rom_session(char const *name)
			{
				try {
					char args[Session::Name::MAX_SIZE];
					snprintf(args, sizeof(args), "ram_quota=4K, filename=\"%s\"", name);
					return static_cap_cast<Rom_session>(_local_rom_service.session(args));
				} catch (Genode::Parent::Service_denied) {
					PERR("Lookup for ROM module \"%s\" failed", name);
					throw;
				}
			}

		public:

			Child(const char *binary_name,
			      const char *label,
			      Rpc_entrypoint &ep,
			      Ram_session_client &ram_session_client,
			      size_t ram_quota,
			      Service_registry &parent_services,
			      Service &local_rom_service,
			      Service &local_nitpicker_service,
			      int max_width, int max_height)
			:
				_label(label),
				_ep(ep),
				_resources(_label.string, ram_session_client, ram_quota),
				_parent_services(parent_services),
				_local_nitpicker_service(local_nitpicker_service),
				_local_rom_service(local_rom_service),
				_binary_rom_session(_rom_session(binary_name)),
				_binary_policy("binary", _binary_rom_session.dataspace(), &_ep),
				_labeling_policy(_label.string),
				_max_width(max_width), _max_height(max_height),
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

			const char *name() const { return _label.string; }

			void filter_session_args(char const *service, char *args, size_t args_len)
			{
				_labeling_policy.filter_session_args(0, args, args_len);

				if (!strcmp(service, "Nitpicker")) {

					/*
					 * Restrict the child's framebuffer size to the maximum
					 * size given by the client.
					 */

					if (_max_width > -1) {
						int fb_width = Arg_string::find_arg(args, "fb_width" ).long_value(_max_width);
						if (!Arg_string::set_arg(args, args_len, "fb_width",
						                         min(fb_width, _max_width))) {
							PERR("could not set fb_width argument");
						}
					}
					if (_max_height > -1) {
						int fb_height = Arg_string::find_arg(args, "fb_height" ).long_value(_max_height);
						if (!Arg_string::set_arg(args, args_len, "fb_height",
						                         min(fb_height, _max_height))) {
							PERR("could not set fb_height argument");
						}
					}
				}
			}

			Service *resolve_session_request(const char *name,
			                                 const char *args)
			{
				Service *service = 0;

				if ((service = _binary_policy.resolve_session_request(name, args)))
					return service;

				if (!strcmp(name, "Nitpicker"))
					return &_local_nitpicker_service;

				if (!strcmp(name, "ROM"))
					return &_local_rom_service;

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
