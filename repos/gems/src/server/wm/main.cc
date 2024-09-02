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
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <util/reconstructible.h>
#include <util/xml_node.h>

/* local includes */
#include <gui.h>
#include <report_forwarder.h>
#include <rom_forwarder.h>

namespace Wm {

	class Main;

}


struct Wm::Main : Pointer::Tracker
{
	Genode::Env &env;

	Genode::Heap heap { env.ram(), env.rm() };

	/* currently focused window, reported by the layouter */
	Attached_rom_dataspace focus_rom { env, "focus" };

	/* resize requests, issued by the layouter */
	Attached_rom_dataspace resize_request_rom { env, "resize_request" };

	/* pointer position to be consumed by the layouter */
	Reporter pointer_reporter = { env, "pointer" };

	/* list of present windows, to be consumed by the layouter */
	Reporter window_list_reporter = { env, "window_list" };

	/* request to the layouter to set the focus */
	Reporter focus_request_reporter = { env, "focus_request" };

	Window_registry window_registry { heap, window_list_reporter };

	Gui::Connection focus_gui_session { env };

	Gui::Root gui_root { env, window_registry,
	                     *this, focus_request_reporter,
	                     focus_gui_session };

	void handle_focus_update()
	{
		focus_rom.update();

		focus_rom.xml().with_optional_sub_node("window", [&] (Xml_node const &window) {

			unsigned const win_id = window.attribute_value("id", 0u);

			if (win_id) {
				try {
					Gui::Session_capability session_cap =
						gui_root.lookup_gui_session(win_id);

					focus_gui_session.focus(session_cap);
				} catch (...) { }
			}
		});
	}

	Genode::Signal_handler<Main> focus_handler = {
		env.ep(), *this, &Main::handle_focus_update };

	void handle_resize_request_update()
	{
		resize_request_rom.update();

		resize_request_rom.xml().for_each_sub_node("window", [&] (Xml_node window) {

			unsigned const
				win_id = window.attribute_value("id",     0U),
				width  = window.attribute_value("width",  0U),
				height = window.attribute_value("height", 0U);

			gui_root.request_resize(win_id, Area(width, height));
		});
	}

	Genode::Signal_handler<Main> resize_request_handler =
		{ env.ep(), *this, &Main::handle_resize_request_update };

	Report_forwarder _report_forwarder { env, heap };
	Rom_forwarder    _rom_forwarder    { env, heap };

	Genode::Signal_handler<Main> _update_pointer_report_handler =
		{ env.ep(), *this, &Main::_handle_update_pointer_report };

	void _handle_update_pointer_report()
	{
		Pointer::Position pos = gui_root.last_observed_pointer_pos();

		Reporter::Xml_generator xml(pointer_reporter, [&] ()
		{
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

	Main(Genode::Env &env) : env(env)
	{
		pointer_reporter.enabled(true);

		/* initially report an empty window list */
		window_list_reporter.enabled(true);
		Genode::Reporter::Xml_generator xml(window_list_reporter, [&] () { });

		focus_request_reporter.enabled(true);

		focus_rom.sigh(focus_handler);
		resize_request_rom.sigh(resize_request_handler);
	}
};


/***************
 ** Component **
 ***************/

Genode::size_t Component::stack_size() {
	return 16*1024*sizeof(long); }

void Component::construct(Genode::Env &env) {
		static Wm::Main desktop(env); }
