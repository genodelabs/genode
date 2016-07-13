/*
 * \brief  Dialog
 * \author Norman Feske
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FADING_DIALOG_H_
#define _FADING_DIALOG_H_

/* gems includes */
#include <gems/report_rom_slave.h>
#include <gems/local_reporter.h>

/* local includes */
#include <nit_fader_slave.h>
#include <menu_view_slave.h>
#include <dialog_nitpicker.h>
#include <types.h>

namespace Launcher
{
	struct Dialog_generator { virtual void generate_dialog(Xml_generator &) = 0; };

	struct Hover_handler { virtual void hover_changed(Xml_node hover) = 0; };

	typedef Dialog_nitpicker_session::Input_event_handler Input_event_handler;

	class Fading_dialog;

	class Dialog_model
	{
		private:

			bool _up_to_date = true;

			friend class Fading_dialog;

		public:

			void dialog_changed() { _up_to_date = false; }
	};
}



class Launcher::Fading_dialog : private Input_event_handler
{
	private:

		Rom_session_capability _dialog_rom;

		/* dialog reported locally */
		Capability<Report::Session> _dialog_report;

		Rom_session_client _hover_rom;

		Lazy_volatile_object<Attached_dataspace> _hover_ds;

		/* hovered element reported by menu view */
		Capability<Report::Session> _hover_report;

		Local_reporter _dialog_reporter { "dialog", _dialog_report };

		Input_event_handler &_dialog_input_event_handler;

		Hover_handler &_hover_handler;

		Dialog_generator &_dialog_generator;

		Dialog_model &_dialog_model;

		void _update_dialog()
		{
			bool const dialog_needs_update = !_dialog_model._up_to_date;

			_dialog_model._up_to_date = false;

			if (dialog_needs_update) {
				Local_reporter::Xml_generator xml(_dialog_reporter, [&] ()
				{
					_dialog_generator.generate_dialog(xml);
				});
			}
		}

		/**
		 * Input_event_handler interface
		 */
		bool handle_input_event(Input::Event const &ev) override
		{
			bool const forward_event = _dialog_input_event_handler.handle_input_event(ev);
			_update_dialog();
			return forward_event;
		}

		void _handle_hover_update(unsigned)
		{
			try {
				if (!_hover_ds.constructed() || _hover_rom.update() == false) {
					if (_hover_ds.constructed())
						_hover_ds->invalidate();
					_hover_ds.construct(_hover_rom.dataspace());
				}

				Xml_node hover(_hover_ds->local_addr<char>());

				_hover_handler.hover_changed(hover);

				bool const dialog_needs_update = !_dialog_model._up_to_date;

				_dialog_model._up_to_date = true;

				if (dialog_needs_update) {
					Local_reporter::Xml_generator xml(_dialog_reporter, [&] ()
					{
						_dialog_generator.generate_dialog(xml);
					});
				}

			} catch (...) {
				Genode::warning("no menu hover model available");
			}
		}

		Signal_rpc_member<Fading_dialog> _hover_update_dispatcher;

	private:

		/**
		 * Local nitpicker service to be handed out to the menu view slave
		 */
		struct Nitpicker_service : Genode::Service
		{
			Server::Entrypoint &ep;
			Rpc_entrypoint &child_ep;

			/* connection to real nitpicker */
			Nitpicker::Connection connection { "menu" };

			Dialog_nitpicker_session wrapper_session;

			Capability<Nitpicker::Session> session_cap { child_ep.manage(&wrapper_session) };

			Nitpicker_service(Server::Entrypoint &ep,
			                  Rpc_entrypoint &child_ep,
			                  Dialog_nitpicker_session::Input_event_handler &ev_handler)
			:
				Genode::Service(Nitpicker::Session::service_name()),
				ep(ep), child_ep(child_ep),
				wrapper_session(connection, ep, ev_handler)
			{ }

			/*******************************
			 ** Genode::Service interface **
			 *******************************/

			Genode::Session_capability
			session(const char *, Genode::Affinity const &) override
			{
				return session_cap;
			}

			void upgrade(Genode::Session_capability, const char *args) override
			{
				Genode::log("upgrade called args: '", args, "'");
			}

			void close(Genode::Session_capability) override { }
		};

		/*
		 * Entrypoint for the fader slave
		 *
		 * This entrypoint is used for handling the parent interface for the
		 * fader slave and for providing the wrapped nitpicker service to the
		 * slave. The latter cannot be provided by the main entrypoint because
		 * during the construction of the 'nit_fader_slave' (executed in the
		 * context of the main entrypoint), the slave tries to create a
		 * nitpicker session (to be answered with the wrapped session).
		 */
		size_t   const _fader_slave_ep_stack_size = 4*1024*sizeof(addr_t);
		Rpc_entrypoint _fader_slave_ep;

		Nitpicker_service _nitpicker_service;
		Nit_fader_slave   _nit_fader_slave;
		Menu_view_slave   _menu_view_slave;

		bool _visible = false;

	public:

		typedef Menu_view_slave::Position Position;

		/**
		 * Constructor
		 *
		 * \param ep   main entrypoint, used for managing the local input
		 *             session provided (indirectly through the wrapped
		 *             nitpicker session) to the menu view
		 * \param cap  capability session to be used for creating the
		 *             slave entrypoints
		 * \param ram  RAM session where to draw the memory for providing
		 *             configuration data to the slave processes
		 */
		Fading_dialog(Server::Entrypoint  &ep,
		              Cap_session         &cap,
		              Ram_session         &ram,
		              Dataspace_capability ldso_ds,
		              Report_rom_slave    &report_rom_slave,
		              char          const *dialog_name,
		              char          const *hover_name,
		              Input_event_handler &input_event_handler,
		              Hover_handler       &hover_handler,
		              Dialog_generator    &dialog_generator,
		              Dialog_model        &dialog_model,
		              Position             initial_position)
		:
			_dialog_rom(report_rom_slave.rom_session(dialog_name)),
			_dialog_report(report_rom_slave.report_session(dialog_name)),
			_hover_rom(report_rom_slave.rom_session(hover_name)),
			_hover_report(report_rom_slave.report_session(hover_name)),
			_dialog_input_event_handler(input_event_handler),
			_hover_handler(hover_handler),
			_dialog_generator(dialog_generator),
			_dialog_model(dialog_model),
			_hover_update_dispatcher(ep, *this, &Fading_dialog::_handle_hover_update),
			_fader_slave_ep(&cap, _fader_slave_ep_stack_size, "nit_fader"),
			_nitpicker_service(ep, _fader_slave_ep, *this),
			_nit_fader_slave(_fader_slave_ep, ram, _nitpicker_service, ldso_ds),
			_menu_view_slave(cap, ram, ldso_ds,
			                 _nit_fader_slave.nitpicker_session("menu"),
			                 _dialog_rom, _hover_report, initial_position)
		{
			Rom_session_client(_hover_rom).sigh(_hover_update_dispatcher);
		}

		void update()
		{
			Local_reporter::Xml_generator xml(_dialog_reporter, [&] ()
			{
				_dialog_generator.generate_dialog(xml);
			});
		}

		void visible(bool visible)
		{
			_nit_fader_slave.visible(visible);
			_visible = visible;
		}

		bool visible() const { return _visible; }

		void position(Position position) { _menu_view_slave.position(position); }
};

#endif /* _FADING_DIALOG_H_ */
