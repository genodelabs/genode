/*
 * \brief  Slave for drawing window decorations
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DECORATOR_SLAVE_H_
#define _DECORATOR_SLAVE_H_

namespace Wm {

	class Decorator_slave;

	using Genode::Rom_session_capability;
	using Genode::Capability;
}


class Wm::Decorator_slave
{
	private:

		Genode::Ram_session &_ram;

		class Policy : public Genode::Slave_policy
		{
			private:

				Genode::Service &_nitpicker_service;

				Single_session_service _window_layout_rom_service;
				Single_session_service _pointer_rom_service;
				Single_session_service _hover_report_service;

			protected:

				char const **_permitted_services() const
				{
					static char const *permitted_services[] = {
						"CAP", "LOG", "SIGNAL", "RM", 0 };

					return permitted_services;
				};

			public:

				Policy(Genode::Rpc_entrypoint             &entrypoint,
				       Genode::Ram_session                &ram,
				       Genode::Service                    &nitpicker_service,
				       Rom_session_capability              window_layout_rom,
				       Rom_session_capability              pointer_rom,
				       Genode::Capability<Report::Session> hover_report)
				:
					Slave_policy("decorator", entrypoint, &ram),
					_nitpicker_service(nitpicker_service),
					_window_layout_rom_service("ROM", window_layout_rom),
					_pointer_rom_service("ROM", pointer_rom),
					_hover_report_service("Report", hover_report)

				{ }

				Genode::Service *resolve_session_request(const char *service_name,
				                                         const char *args) override
				{
					using Genode::strcmp;

					if (strcmp(service_name, "Nitpicker") == 0)
						return &_nitpicker_service;

					char label[128];
					Arg_string::find_arg(args, "label").string(label, sizeof(label), "");

					if (strcmp(service_name, "ROM") == 0) {

						if (strcmp(label, "decorator -> window_layout") == 0)
							return &_window_layout_rom_service;

						if (strcmp(label, "decorator -> pointer") == 0)
							return &_pointer_rom_service;
					}

					if (strcmp(service_name, "Report") == 0) {

						if (strcmp(label, "decorator -> hover") == 0)
							return &_hover_report_service;
					}

					return Genode::Slave_policy::resolve_session_request(service_name, args);
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::size_t   const _quota = 4*1024*1024;
		Genode::Slave          _slave;

	public:

		/**
		 * Constructor
		 *
		 * \param ram  RAM session for paying nitpicker sessions created
		 *             by the decorator
		 */
		Decorator_slave(Genode::Cap_session                &cap,
		                Genode::Service                    &nitpicker_service,
		                Genode::Ram_session                &ram,
		                Rom_session_capability              window_layout_rom,
		                Rom_session_capability              pointer_rom,
		                Genode::Capability<Report::Session> hover_report)
		:
			_ram(ram),
			_ep(&cap, _ep_stack_size, "decorator"),
			_policy(_ep, ram, nitpicker_service, window_layout_rom,
			        pointer_rom, hover_report),
			_slave(_ep, _policy, _quota)
		{ }
};

#endif /* _DECORATOR_SLAVE_H_ */
