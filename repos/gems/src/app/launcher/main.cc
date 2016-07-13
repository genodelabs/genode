/*
 * \brief  Launcher
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <os/config.h>
#include <cap_session/connection.h>
#include <decorator/xml_utils.h>
#include <util/volatile_object.h>
#include <os/attached_rom_dataspace.h>
#include <nitpicker_session/connection.h>

/* local includes */
#include <panel_dialog.h>


namespace Launcher { struct Main; }

struct Launcher::Main
{
	Server::Entrypoint &_ep;

	Genode::Dataspace_capability _request_ldso_ds()
	{
		try {
			static Genode::Rom_connection rom("ld.lib.so");
			return rom.dataspace();
		} catch (...) { }
		return Genode::Dataspace_capability();
	}

	Genode::Dataspace_capability _ldso_ds = _request_ldso_ds();

	Genode::Cap_connection _cap;

	char const *_report_rom_config =
		"<config> <rom>"
		"  <policy label=\"menu_dialog\"    report=\"menu_dialog\"/>"
		"  <policy label=\"menu_hover\"     report=\"menu_hover\"/>"
		"  <policy label=\"panel_dialog\"   report=\"panel_dialog\"/>"
		"  <policy label=\"panel_hover\"    report=\"panel_hover\"/>"
		"  <policy label=\"context_dialog\" report=\"context_dialog\"/>"
		"  <policy label=\"context_hover\"  report=\"context_hover\"/>"
		"</rom> </config>";

	Report_rom_slave _report_rom_slave = { _cap, *env()->ram_session(), _report_rom_config };

	/**
	 * Nitpicker session used to perform session-control operations on the
	 * subsystem's nitpicker sessions and to receive global keyboard
	 * shortcuts.
	 */
	Nitpicker::Connection _nitpicker;

	Genode::Attached_dataspace _input_ds { _nitpicker.input()->dataspace() };

	Input::Event const *_ev_buf() { return _input_ds.local_addr<Input::Event>(); }

	Genode::Signal_rpc_member<Main> _input_dispatcher =
		{ _ep, *this, &Main::_handle_input };

	void _handle_input(unsigned);

	unsigned _key_cnt = 0;

	Genode::Signal_rpc_member<Main> _exited_child_dispatcher =
		{ _ep, *this, &Main::_handle_exited_child };

	Subsystem_manager _subsystem_manager { _ep, _cap, _exited_child_dispatcher,
	                                       _ldso_ds };

	Panel_dialog _panel_dialog { _ep, _cap, *env()->ram_session(), _ldso_ds,
	                             *env()->heap(),
	                             _report_rom_slave, _subsystem_manager, _nitpicker };

	void _handle_config(unsigned);

	void _handle_exited_child(unsigned)
	{
		auto kill_child_fn = [&] (Label const &label) { _panel_dialog.kill(label); };

		_subsystem_manager.for_each_exited_child(kill_child_fn);
	}

	Label _focus_prefix;

	Genode::Attached_rom_dataspace _focus_rom { "focus" };

	void _handle_focus_update(unsigned);

	Genode::Signal_rpc_member<Main> _focus_update_dispatcher =
		{ _ep, *this, &Main::_handle_focus_update };

	/**
	 * Constructor
	 */
	Main(Server::Entrypoint &ep) : _ep(ep)
	{
		_nitpicker.input()->sigh(_input_dispatcher);
		_focus_rom.sigh(_focus_update_dispatcher);

		_handle_config(0);

		_panel_dialog.visible(true);
	}
};


void Launcher::Main::_handle_config(unsigned)
{
	config()->reload();

	_focus_prefix = config()->xml_node().attribute_value("focus_prefix", Label());

	_panel_dialog.update(config()->xml_node());
}


void Launcher::Main::_handle_input(unsigned)
{
	unsigned const num_ev = _nitpicker.input()->flush();

	for (unsigned i = 0; i < num_ev; i++) {

		Input::Event const &e = _ev_buf()[i];

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
	}
}


void Launcher::Main::_handle_focus_update(unsigned)
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


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "desktop_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Launcher::Main desktop(ep);
	}
}
