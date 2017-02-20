/*
 * \brief  Launcher
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <decorator/xml_utils.h>
#include <util/reconstructible.h>
#include <base/attached_rom_dataspace.h>
#include <nitpicker_session/connection.h>

/* local includes */
#include <panel_dialog.h>

namespace Launcher {

	using namespace Genode;
	struct Main;
}


struct Launcher::Main
{
	Env &_env;

	char const *_report_rom_config =
		"<config>"
		"  <policy label=\"menu_dialog\"    report=\"menu_dialog\"/>"
		"  <policy label=\"menu_hover\"     report=\"menu_hover\"/>"
		"  <policy label=\"panel_dialog\"   report=\"panel_dialog\"/>"
		"  <policy label=\"panel_hover\"    report=\"panel_hover\"/>"
		"  <policy label=\"context_dialog\" report=\"context_dialog\"/>"
		"  <policy label=\"context_hover\"  report=\"context_hover\"/>"
		"</config>";

	Report_rom_slave _report_rom_slave {
		_env.pd(), _env.rm(), _env.ram_session_cap(), _report_rom_config };

	/**
	 * Nitpicker session used to perform session-control operations on the
	 * subsystem's nitpicker sessions and to receive global keyboard
	 * shortcuts.
	 */
	Nitpicker::Connection _nitpicker { _env };

	Signal_handler<Main> _input_handler =
		{ _env.ep(), *this, &Main::_handle_input };

	void _handle_input();

	unsigned _key_cnt = 0;

	Signal_handler<Main> _exited_child_handler =
		{ _env.ep(), *this, &Main::_handle_exited_child };

	Attached_rom_dataspace _config { _env, "config" };

	static size_t _ram_preservation(Xml_node config)
	{
		char const * const node_name = "preservation";

		if (config.has_sub_node(node_name)) {

			Xml_node const node = config.sub_node(node_name);
			if (node.attribute_value("name", Genode::String<16>()) == "RAM")
				return node.attribute_value("quantum", Genode::Number_of_bytes());
		}

		return 0;
	}

	Subsystem_manager _subsystem_manager { _env,
	                                       _ram_preservation(_config.xml()),
	                                       _exited_child_handler };

	Heap _heap { _env.ram(), _env.rm() };

	Panel_dialog _panel_dialog { _env, _heap, _report_rom_slave,
	                             _subsystem_manager, _nitpicker };

	void _handle_config();

	void _handle_exited_child()
	{
		auto kill_child_fn = [&] (Label const &label) { _panel_dialog.kill(label); };

		_subsystem_manager.for_each_exited_child(kill_child_fn);
	}

	Label _focus_prefix;

	Genode::Attached_rom_dataspace _focus_rom { _env, "focus" };

	void _handle_focus_update();

	Signal_handler<Main> _focus_update_handler =
		{ _env.ep(), *this, &Main::_handle_focus_update };

	/**
	 * Constructor
	 */
	Main(Env &env) : _env(env)
	{
		_nitpicker.input()->sigh(_input_handler);
		_focus_rom.sigh(_focus_update_handler);

		_handle_config();

		_panel_dialog.visible(true);
	}
};


void Launcher::Main::_handle_config()
{
	_config.update();

	_focus_prefix = _config.xml().attribute_value("focus_prefix", Label());

	try {
		_panel_dialog.update(_config.xml()); }
	catch (Allocator::Out_of_memory) {
		error("out of memory while applying configuration"); }
}


void Launcher::Main::_handle_input()
{
	_nitpicker.input()->for_each_event([&] (Input::Event const &e) {
		if (e.type() == Input::Event::PRESS)   _key_cnt++;
		if (e.type() == Input::Event::RELEASE) _key_cnt--;

		/*
		 * The _key_cnt can become 2 only when the global key (as configured
		 * in the nitpicker config) is pressed together with another key.
		 * Hence, the following condition triggers on key combinations with
		 * the global modifier key, whatever the global modifier key is.
		 */
		if (e.type() == Input::Event::PRESS && _key_cnt == 2) {

			if (e.keycode() == Input::KEY_TAB)
				_panel_dialog.focus_next();
		}
	});
}


void Launcher::Main::_handle_focus_update()
{
	try {
		_focus_rom.update();

		Xml_node focus_node(_focus_rom.local_addr<char>());

		/*
		 * Propagate focus information to panel such that the focused
		 * subsystem gets highlighted.
		 */
		Label label = focus_node.attribute_value("label", Label());

		size_t const prefix_len = Genode::strlen(_focus_prefix.string());
		if (!Genode::strcmp(_focus_prefix.string(), label.string(), prefix_len)) {
			label = Label(label.string() + prefix_len);

		} else {

			/*
			 * A foreign nitpicker client not started by ourself has the focus.
			 */
			label = Label();
		}

		_panel_dialog.focus_changed(label);

	} catch (...) {
		Genode::warning("no focus model available");
	}
}


void Component::construct(Genode::Env &env) { static Launcher::Main main(env); }
