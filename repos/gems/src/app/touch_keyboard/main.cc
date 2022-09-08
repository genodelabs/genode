/*
 * \brief  Simple touch-screen keyboard
 * \author Norman Feske
 * \date   2022-01-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/session_object.h>
#include <base/attached_rom_dataspace.h>
#include <sandbox/sandbox.h>
#include <os/dynamic_rom_session.h>
#include <os/reporter.h>
#include <os/buffered_xml.h>
#include <event_session/connection.h>

/* local includes */
#include <gui.h>
#include <report.h>
#include <touch_keyboard_dialog.h>
#include <child_state.h>

namespace Touch_keyboard { struct Main; }

struct Touch_keyboard::Main : Sandbox::Local_service_base::Wakeup,
                              Sandbox::State_handler,
                              Gui::Input_event_handler,
                              Dialog::Event_emitter
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };
	Attached_rom_dataspace _layout { _env, "layout" };

	int      _xpos       = 0;
	int      _ypos       = 0;
	unsigned _min_width  = 0;
	unsigned _min_height = 0;

	Registry<Child_state> _children { };

	Child_state _menu_view_child_state { _children, "menu_view",
	                                     Ram_quota { 10*1024*1024 },
	                                     Cap_quota { 200 } };
	/**
	 * Sandbox::State_handler
	 */
	void handle_sandbox_state() override
	{
		/* obtain current sandbox state */
		Buffered_xml state(_heap, "state", [&] (Xml_generator &xml) {
			_sandbox.generate_state_report(xml);
		});

		bool reconfiguration_needed = false;

		state.with_xml_node([&] (Xml_node state) {
			state.for_each_sub_node("child", [&] (Xml_node const &child) {
				if (_menu_view_child_state.apply_child_state_report(child))
					reconfiguration_needed = true; }); });

		if (reconfiguration_needed)
			_update_sandbox_config();
	}

	Sandbox _sandbox { _env, *this };

	typedef Sandbox::Local_service<Gui::Session_component> Gui_service;

	Gui_service _gui_service { _sandbox, *this };

	typedef Sandbox::Local_service<Dynamic_rom_session> Rom_service;

	Rom_service _rom_service { _sandbox, *this };

	typedef Sandbox::Local_service<Report::Session_component> Report_service;

	Report_service _report_service { _sandbox, *this };

	void _handle_hover(Xml_node const &node)
	{
		Input::Seq_number hover_seq { node.attribute_value("seq_number", 0U) };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &dialog) {
			_dialog.handle_hover(hover_seq, dialog); });
	}

	Report::Session_component::Xml_handler<Main>
		_hover_handler { *this, &Main::_handle_hover };

	Dialog _dialog { _env.ep(), _env.ram(), _env.rm(), _heap, *this };

	void _generate_sandbox_config(Xml_generator &xml) const
	{
		xml.node("report", [&] () {
			xml.attribute("child_ram",  "yes");
			xml.attribute("child_caps", "yes");
			xml.attribute("delay_ms", 20*1000);
		});
		xml.node("parent-provides", [&] () {

			auto service_node = [&] (char const *name) {
				xml.node("service", [&] () {
					xml.attribute("name", name); }); };

			service_node("ROM");
			service_node("CPU");
			service_node("PD");
			service_node("LOG");
			service_node("File_system");
			service_node("Gui");
			service_node("Timer");
			service_node("Report");
		});

		xml.node("start", [&] () {
			_menu_view_child_state.gen_start_node_content(xml);

			xml.node("config", [&] () {
				xml.attribute("xpos", _xpos);
				xml.attribute("ypos", _ypos);

				if (_min_width)  xml.attribute("width",  _min_width);
				if (_min_height) xml.attribute("height", _min_height);

				xml.node("report", [&] () {
					xml.attribute("hover", "yes"); });

				xml.node("libc", [&] () {
					xml.attribute("stderr", "/dev/log"); });

				xml.node("vfs", [&] () {
					xml.node("tar", [&] () {
						xml.attribute("name", "menu_view_styles.tar"); });
					xml.node("dir", [&] () {
						xml.attribute("name", "dev");
						xml.node("log", [&] () { });
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "fonts");
						xml.node("fs", [&] () {
							xml.attribute("label", "fonts");
						});
					});
				});
			});

			xml.node("route", [&] () {

				xml.node("service", [&] () {
					xml.attribute("name", "ROM");
					xml.attribute("label", "dialog");
					xml.node("local", [&] () { });
				});

				xml.node("service", [&] () {
					xml.attribute("name", "Report");
					xml.attribute("label", "hover");
					xml.node("local", [&] () { });
				});

				xml.node("service", [&] () {
					xml.attribute("name", "Gui");
					xml.node("local", [&] () { });
				});

				xml.node("service", [&] () {
					xml.attribute("name", "File_system");
					xml.attribute("label", "fonts");
					xml.node("parent", [&] () {
						xml.attribute("label", "fonts"); });
				});

				xml.node("any-service", [&] () {
					xml.node("parent", [&] () { }); });
			});
		});
	}

	/**
	 * Sandbox::Local_service_base::Wakeup interface
	 */
	void wakeup_local_service() override
	{
		_rom_service.for_each_requested_session([&] (Rom_service::Request &request) {

			if (request.label == "menu_view -> dialog")
				request.deliver_session(_dialog.rom_session);
			else
				request.deny();
		});

		_report_service.for_each_requested_session([&] (Report_service::Request &request) {

			if (request.label == "menu_view -> hover") {
				Report::Session_component &session = *new (_heap)
					Report::Session_component(_env, _hover_handler,
					                          _env.ep(),
					                          request.resources, "", request.diag);
				request.deliver_session(session);
			}
		});

		_report_service.for_each_session_to_close([&] (Report::Session_component &session) {

			destroy(_heap, &session);
			return Report_service::Close_response::CLOSED;
		});

		_gui_service.for_each_requested_session([&] (Gui_service::Request &request) {

			Gui::Session_component &session = *new (_heap)
				Gui::Session_component(_env, *this, _global_input_seq_number, _env.ep(),
				                       request.resources, "", request.diag);

			request.deliver_session(session);
		});

		_gui_service.for_each_upgraded_session([&] (Gui::Session_component &session,
		                                                  Session::Resources const &amount) {
			session.upgrade(amount);
			return Gui_service::Upgrade_response::CONFIRMED;
		});

		_gui_service.for_each_session_to_close([&] (Gui::Session_component &session) {

			destroy(_heap, &session);
			return Gui_service::Close_response::CLOSED;
		});
	}

	Event::Connection _event_connection { _env };

	/**
	 * Dialog::Event_emitter interface
	 */
	void emit_characters(Dialog::Emit const &characters) override
	{
		_event_connection.with_batch([&] (Event::Session_client::Batch &batch) {

			Utf8_ptr utf8_ptr(characters.string());

			for (; utf8_ptr.complete(); utf8_ptr = utf8_ptr.next()) {

				Codepoint c = utf8_ptr.codepoint();
				batch.submit( Input::Press_char { Input::KEY_UNKNOWN, c } );
				batch.submit( Input::Release    { Input::KEY_UNKNOWN } );
			}
		});
	}

	Input::Seq_number _global_input_seq_number { };

	/**
	 * Gui::Input_event_handler interface
	 */
	void handle_input_event(Input::Event const &event) override
	{
		_dialog.handle_input_event(_global_input_seq_number, event);
	}

	void _handle_config()
	{
		_config.update();
		_layout.update();

		Xml_node const config = _config.xml();

		_xpos = (int)config.attribute_value("xpos", 0L);
		_ypos = (int)config.attribute_value("ypos", 0L);

		_min_width  = config.attribute_value("min_width",  0U);
		_min_height = config.attribute_value("min_height", 0U);

		_dialog.configure(_layout.xml());
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	void _update_sandbox_config()
	{
		Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
			_generate_sandbox_config(xml); } };

		config.with_xml_node([&] (Xml_node const &config) {
			_sandbox.apply_config(config); });
	}

	Main(Env &env)
	:
		_env(env)
	{
		_config.sigh(_config_handler);
		_layout.sigh(_config_handler);
		_handle_config();
		_update_sandbox_config();
	}
};


void Component::construct(Genode::Env &env) { static Touch_keyboard::Main main(env); }

