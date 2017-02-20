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
#include <nitpicker_session/client.h>
#include <framebuffer_session/client.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <util/reconstructible.h>
#include <util/xml_node.h>

/* local includes */
#include <nitpicker.h>

namespace Wm {

	class Main;

	using Genode::size_t;
	using Genode::env;
	using Genode::Rom_session_client;
	using Genode::Rom_connection;
	using Genode::Xml_node;
	using Genode::Attached_rom_dataspace;
}


struct Wm::Main
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

	Nitpicker::Connection focus_nitpicker_session { env };

	Nitpicker::Root nitpicker_root { env, window_registry,
	                                 heap, env.ram_session_cap(),
	                                 pointer_reporter, focus_request_reporter,
	                                 focus_nitpicker_session };

	void handle_focus_update()
	{
		try {
			focus_rom.update();
			if (!focus_rom.valid())
				return;

			unsigned long win_id = 0;

			Xml_node(focus_rom.local_addr<char>()).sub_node("window")
				.attribute("id").value(&win_id);

			if (win_id) {
				Nitpicker::Session_capability session_cap =
					nitpicker_root.lookup_nitpicker_session(win_id);

				focus_nitpicker_session.focus(session_cap);
			}

		} catch (...) {
			Genode::warning("no focus model available");
		}
	}

	Genode::Signal_handler<Main> focus_handler = {
		env.ep(), *this, &Main::handle_focus_update };

	void handle_resize_request_update()
	{
		try {
			resize_request_rom.update();
			if (!resize_request_rom.valid())
				return;

			char const * const node_type = "window";

			Xml_node window =
				Xml_node(resize_request_rom.local_addr<char>()).sub_node(node_type);

			for (;;) {
				unsigned long win_id = 0, width = 0, height = 0;

				window.attribute("id")    .value(&win_id);
				window.attribute("width") .value(&width);
				window.attribute("height").value(&height);

				nitpicker_root.request_resize(win_id, Area(width, height));

				if (window.last(node_type))
					break;

				window = window.next(node_type);
			}

		} catch (...) { /* no resize-request model available */ }
	}

	Genode::Signal_handler<Main> resize_request_handler =
		{ env.ep(), *this, &Main::handle_resize_request_update };

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
