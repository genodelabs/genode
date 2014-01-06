/*
 * \brief  Slave for managing the window layout
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WINDOW_LAYOUTER_SLAVE_H_
#define _WINDOW_LAYOUTER_SLAVE_H_

namespace Wm {

	class Window_layouter_slave;

	using Genode::Rom_session_capability;
	using Genode::Capability;
}


class Wm::Window_layouter_slave
{
	private:

		Genode::Ram_session &_ram;

		class Policy : public Genode::Slave_policy
		{
			private:

				Single_session_service _window_list_rom_service;
				Single_session_service _hover_rom_service;
				Single_session_service _input_service;
				Single_session_service _window_layout_report_service;
				Single_session_service _resize_request_report_service;
				Single_session_service _focus_report_service;

			protected:

				char const **_permitted_services() const
				{
					static char const *permitted_services[] = {
						"CAP", "LOG", "SIGNAL", "RM", "Timer", 0 };

					return permitted_services;
				};

			public:

				Policy(Genode::Rpc_entrypoint     &entrypoint,
				       Genode::Ram_session        &ram,
				       Rom_session_capability      window_list_rom,
				       Rom_session_capability      hover_rom,
				       Input::Session_capability   input,
				       Capability<Report::Session> window_layout_report,
				       Capability<Report::Session> resize_request_report,
				       Capability<Report::Session> focus_report)
				:
					Slave_policy("floating_window_layouter", entrypoint, &ram),
					_window_list_rom_service("ROM", window_list_rom),
					_hover_rom_service("ROM", hover_rom),
					_input_service("Input", input),
					_window_layout_report_service("Report", window_layout_report),
					_resize_request_report_service("Report", resize_request_report),
					_focus_report_service("Report", focus_report)
				{ }

				Genode::Service *resolve_session_request(const char *service_name,
				                                         const char *args) override
				{
					using Genode::strcmp;

					char label[128];
					Arg_string::find_arg(args, "label").string(label, sizeof(label), "");

					if (strcmp(service_name, "ROM") == 0) {

						if (strcmp(label, "floating_window_layouter -> window_list") == 0)
							return &_window_list_rom_service;

						if (strcmp(label, "floating_window_layouter -> hover") == 0)
							return &_hover_rom_service;
					}

					if (strcmp(service_name, "Report") == 0) {

						if (strcmp(label, "floating_window_layouter -> window_layout") == 0)
							return &_window_layout_report_service;

						if (strcmp(label, "floating_window_layouter -> resize_request") == 0)
							return &_resize_request_report_service;

						if (strcmp(label, "floating_window_layouter -> focus") == 0)
							return &_focus_report_service;
					}

					if (strcmp(service_name, "Input") == 0)
						return &_input_service;

					return Genode::Slave_policy::resolve_session_request(service_name, args);
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::size_t   const _quota = 1*1024*1024;
		Genode::Slave          _slave;

	public:

		/**
		 * Constructor
		 */
		Window_layouter_slave(Genode::Cap_session        &cap,
		                      Genode::Ram_session        &ram,
		                      Rom_session_capability      window_list_rom,
		                      Rom_session_capability      hover_rom,
		                      Input::Session_capability   input,
		                      Capability<Report::Session> window_layout_report,
		                      Capability<Report::Session> resize_request_report,
		                      Capability<Report::Session> focus_report)
		:
			_ram(ram),
			_ep(&cap, _ep_stack_size, "floating_window_layouter"),
			_policy(_ep, ram, window_list_rom, hover_rom, input,
			        window_layout_report, resize_request_report, focus_report),
			_slave(_ep, _policy, _quota)
		{ }
};

#endif /* _WINDOW_LAYOUTER_SLAVE_H_ */
