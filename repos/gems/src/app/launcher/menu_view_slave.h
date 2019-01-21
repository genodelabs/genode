/*
 * \brief  Slave used for presenting the menu
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MENU_VIEW_SLAVE_H_
#define _MENU_VIEW_SLAVE_H_

/* Genode includes */
#include <os/static_parent_services.h>
#include <os/slave.h>
#include <nitpicker_session/nitpicker_session.h>
#include <file_system_session/file_system_session.h>

/* gems includes */
#include <os/single_session_service.h>

/* local includes */
#include <types.h>

namespace Launcher { class Menu_view_slave; }


class Launcher::Menu_view_slave
{
	public:

		typedef Surface_base::Point Position;

	private:

		struct Policy_base
		{
			Genode::Static_parent_services<Genode::Cpu_session,
			                               Genode::Pd_session,
			                               Genode::Rom_session,
			                               Genode::Log_session,
			                               File_system::Session,
			                               Timer::Session>
				_parent_services;

			Policy_base(Env &env) : _parent_services(env) { }
		};

		class Policy : Policy_base, public Genode::Slave::Policy
		{
			private:

				Genode::Single_session_service<Nitpicker::Session>  _nitpicker;
				Genode::Single_session_service<Genode::Rom_session> _dialog_rom;
				Genode::Single_session_service<Report::Session>     _hover_report;

				Position _position;

			private:

				void _configure(Position pos)
				{
					char config[1024];

					snprintf(config, sizeof(config),
					         "<config xpos=\"%d\" ypos=\"%d\">\n"
					         "  <report hover=\"yes\"/>\n"
					         "  <libc stderr=\"/dev/log\"/>\n"
					         "  <vfs>\n"
					         "    <tar name=\"menu_view_styles.tar\" />\n"
					         "    <dir name=\"fonts\"> <fs label=\"fonts\"/> </dir>\n"
					         "  </vfs>\n"
					         "</config>",
					         pos.x(), pos.y());

					configure(config);
				}

				static Name              _name()  { return "menu_view"; }
				static Genode::Ram_quota _quota() { return { 6*1024*1024 }; }
				static Genode::Cap_quota _caps()  { return { 100 }; }

				Service &_matching_service(Genode::Service::Name const &service,
				                           Genode::Session_label const &label)
				{
					if (service == "Nitpicker")
						return _nitpicker.service();

					if ((service == "ROM") && (label == "menu_view -> dialog"))
						return _dialog_rom.service();

					if ((service == "Report") && (label == "menu_view -> hover"))
						return _hover_report.service();

					throw Genode::Service_denied();
				}

			public:

				Policy(Genode::Env                   &env,
				       Genode::Rpc_entrypoint        &ep,
				       Capability<Nitpicker::Session> nitpicker_session,
				       Capability<Rom_session>        dialog_rom_session,
				       Capability<Report::Session>    hover_report_session,
				       Position                       position)
				:
					Policy_base(env),
					Genode::Slave::Policy(env, _name(), _name(),
					                      Policy_base::_parent_services,
					                      ep, _caps(), _quota()),
					_nitpicker(env.rm(), nitpicker_session),
					_dialog_rom(dialog_rom_session),
					_hover_report(hover_report_session),
					_position(position)
				{
					_configure(position);
				}

				void position(Position pos) { _configure(pos); }

				Route resolve_session_request(Genode::Service::Name const &name,
				                              Genode::Session_label const &label) override
				{
					try {
						return Route { .service = _matching_service(name, label),
						               .label   = label,
						               .diag    = Genode::Session::Diag() };
					}
					catch (Genode::Service_denied) { }

					return Genode::Slave::Policy::resolve_session_request(name, label);
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::Child          _child;

	public:

		/**
		 * Constructor
		 */
		Menu_view_slave(Genode::Env                   &env,
		                Capability<Nitpicker::Session> nitpicker_session,
		                Capability<Rom_session>        dialog_rom_session,
		                Capability<Report::Session>    hover_report_session,
		                Position                       initial_position)
		:
			_ep(&env.pd(), _ep_stack_size, "nit_fader"),
			_policy(env, _ep,
			        nitpicker_session, dialog_rom_session,
			        hover_report_session, initial_position),
			_child(env.rm(), _ep, _policy)
		{ }

		void position(Position position) { _policy.position(position); }
};

#endif /* _NIT_FADER_SLAVE_H_ */
