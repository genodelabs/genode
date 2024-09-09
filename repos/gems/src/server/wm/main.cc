/*
 * \brief  Window manager
 * \author Norman Feske
 * \date   2014-01-06
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <gui_session/client.h>
#include <framebuffer_session/client.h>
#include <base/component.h>

/* local includes */
#include <gui.h>
#include <report_forwarder.h>
#include <rom_forwarder.h>

namespace Wm { class Main; }


struct Wm::Main : Pointer::Tracker
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	/* currently focused window, reported by the layouter */
	Attached_rom_dataspace _focus_rom { _env, "focus" };

	/* resize requests, issued by the layouter */
	Attached_rom_dataspace _resize_request_rom { _env, "resize_request" };

	/* pointer position to be consumed by the layouter */
	Expanding_reporter _pointer_reporter { _env, "pointer", "pointer" };

	/* list of present windows, to be consumed by the layouter */
	Expanding_reporter _window_list_reporter { _env, "window_list", "window_list" };

	Window_registry _window_registry { _heap, _window_list_reporter };

	Gui::Connection _focus_gui_session { _env };

	Gui::Root _gui_root { _env, _window_registry, *this, _focus_gui_session };

	static void _with_win_id_from_xml(Xml_node const &window, auto const &fn)
	{
		if (window.has_attribute("id"))
			fn(Window_registry::Id { window.attribute_value("id", 0u) });
	}

	void _handle_focus_update()
	{
		_focus_rom.update();
		_focus_rom.xml().with_optional_sub_node("window", [&] (Xml_node const &window) {
			_with_win_id_from_xml(window, [&] (Window_registry::Id id) {
				_gui_root.with_gui_session(id, [&] (Capability<Gui::Session> cap) {
					_focus_gui_session.focus(cap); }); }); });
	}

	Signal_handler<Main> _focus_handler {
		_env.ep(), *this, &Main::_handle_focus_update };

	void _handle_resize_request_update()
	{
		_resize_request_rom.update();
		_resize_request_rom.xml().for_each_sub_node("window", [&] (Xml_node const &window) {
			_with_win_id_from_xml(window, [&] (Window_registry::Id id) {
				_gui_root.request_resize(id, Area::from_xml(window)); }); });
	}

	Signal_handler<Main> _resize_request_handler {
		_env.ep(), *this, &Main::_handle_resize_request_update };

	Report_forwarder _report_forwarder { _env, _heap };
	Rom_forwarder    _rom_forwarder    { _env, _heap };

	Signal_handler<Main> _update_pointer_report_handler {
		_env.ep(), *this, &Main::_handle_update_pointer_report };

	void _handle_update_pointer_report()
	{
		Pointer::Position const pos = _gui_root.last_observed_pointer_pos();

		_pointer_reporter.generate([&] (Xml_generator &xml) {
			if (pos.valid) {
				xml.attribute("xpos", pos.value.x);
				xml.attribute("ypos", pos.value.y);
			}
		});
	}

	/**
	 * Pointer::Tracker interface
	 *
	 * This method is called during the event handling, which may affect
	 * multiple 'Pointer::State' instances. Hence, at call time, not all
	 * pointer states may be up to date. To ensure the consistency of all
	 * pointer states when creating the report, we merely schedule a call
	 * of '_handle_update_pointer_report' that is executed after the event
	 * handling is finished.
	 */
	void update_pointer_report() override
	{
		_update_pointer_report_handler.local_submit();
	}

	Main(Env &env) : _env(env)
	{
		/* initially report an empty window list */
		_window_list_reporter.generate([&] (Xml_generator &) { });

		_focus_rom.sigh(_focus_handler);
		_resize_request_rom.sigh(_resize_request_handler);
	}
};


void Component::construct(Genode::Env &env) { static Wm::Main desktop(env); }
