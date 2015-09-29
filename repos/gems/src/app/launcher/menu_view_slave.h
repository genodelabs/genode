/*
 * \brief  Slave used for presenting the menu
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MENU_VIEW_SLAVE_H_
#define _MENU_VIEW_SLAVE_H_

/* Genode includes */
#include <os/slave.h>
#include <nitpicker_session/nitpicker_session.h>

/* gems includes */
#include <gems/single_session_service.h>

/* local includes */
#include <types.h>

namespace Launcher { class Menu_view_slave; }


class Launcher::Menu_view_slave
{
	public:

		typedef Surface_base::Point Position;

	private:

		class Policy : public Genode::Slave_policy
		{
			private:

				Lock     mutable _nitpicker_root_lock { Lock::LOCKED };
				Capability<Root> _nitpicker_root_cap;

				Single_session_service _nitpicker_service;
				Single_session_service _dialog_rom_service;
				Single_session_service _hover_report_service;

				Position _position;

			protected:

				char const **_permitted_services() const
				{
					static char const *permitted_services[] = {
						"ROM", "CAP", "LOG", "SIGNAL", "RM", "Timer", 0 };

					return permitted_services;
				};

			private:

				void _configure(Position pos)
				{
					char config[1024];

					snprintf(config, sizeof(config),
					         "<config xpos=\"%d\" ypos=\"%d\">\n"
					         "  <report hover=\"yes\"/>\n"
					         "  <libc>\n"
					         "    <vfs>\n"
					         "      <tar name=\"menu_view_styles.tar\" />\n"
					         "    </vfs>\n"
					         "  </libc>\n"
					         "</config>",
					         pos.x(), pos.y());

					configure(config);
				}

			public:

				Policy(Genode::Rpc_entrypoint        &entrypoint,
				       Genode::Ram_session           &ram,
				       Capability<Nitpicker::Session> nitpicker_session,
				       Capability<Rom_session>        dialog_rom_session,
				       Capability<Report::Session>    hover_report_session,
				       Position                       position)
				:
					Slave_policy("menu_view", entrypoint, &ram),
					_nitpicker_service(Nitpicker::Session::service_name(), nitpicker_session),
					_dialog_rom_service(Rom_session::service_name(), dialog_rom_session),
					_hover_report_service(Report::Session::service_name(), hover_report_session),
					_position(position)
				{
					_configure(position);
				}

				void position(Position pos)
				{
					_configure(pos);
				}

				Genode::Service *resolve_session_request(const char *service_name,
				                                         const char *args) override
				{
					using Genode::strcmp;

					if (strcmp(service_name, "Nitpicker") == 0)
						return &_nitpicker_service;

					char label[128];
					Arg_string::find_arg(args, "label").string(label, sizeof(label), "");

					if (strcmp(service_name, "ROM") == 0) {

						if (strcmp(label, "menu_view -> dialog") == 0)
							return &_dialog_rom_service;
					}

					if (strcmp(service_name, "Report") == 0) {

						if (strcmp(label, "menu_view -> hover") == 0)
							return &_hover_report_service;
					}

					return Genode::Slave_policy::resolve_session_request(service_name, args);
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::size_t   const _quota = 6*1024*1024;
		Genode::Slave          _slave;

	public:

		/**
		 * Constructor
		 *
		 * \param ep   entrypoint used for child thread
		 * \param ram  RAM session used to allocate the configuration
		 *             dataspace
		 */
		Menu_view_slave(Genode::Cap_session &cap, Genode::Ram_session &ram,
		                Capability<Nitpicker::Session> nitpicker_session,
		                Capability<Rom_session>        dialog_rom_session,
		                Capability<Report::Session>    hover_report_session,
		                Position                       initial_position)
		:
			_ep(&cap, _ep_stack_size, "nit_fader"),
			_policy(_ep, ram, nitpicker_session, dialog_rom_session,
			        hover_report_session, initial_position),
			_slave(_ep, _policy, _quota)
		{ }

		void position(Position position) { _policy.position(position); }
};

#endif /* _NIT_FADER_SLAVE_H_ */
