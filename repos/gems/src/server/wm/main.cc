/*
 * \brief  Window manager
 * \author Norman Feske
 * \date   2014-01-06
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <nitpicker_session/client.h>
#include <framebuffer_session/client.h>
#include <cap_session/connection.h>
#include <os/attached_rom_dataspace.h>
#include <util/volatile_object.h>
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
	Server::Entrypoint ep;

	Genode::Cap_connection cap;

	/* currently focused window, reported by the layouter */
	Attached_rom_dataspace focus_rom { "focus" };

	/* resize requests, issued by the layouter */
	Attached_rom_dataspace resize_request_rom { "resize_request" };

	/* pointer position to be consumed by the layouter */
	Reporter pointer_reporter = { "pointer" };

	/* list of present windows, to be consumed by the layouter */
	Reporter window_list_reporter = { "window_list" };

	/* request to the layouter to set the focus */
	Reporter focus_request_reporter = { "focus_request" };

	Window_registry window_registry { *env()->heap(), window_list_reporter };

	Nitpicker::Root nitpicker_root { ep, window_registry,
	                                 *env()->heap(), env()->ram_session_cap(),
	                                 pointer_reporter, focus_request_reporter };

	Nitpicker::Connection focus_nitpicker_session;

	void handle_focus_update(unsigned)
	{
		try {
			focus_rom.update();

			unsigned long win_id = 0;

			Xml_node(focus_rom.local_addr<char>()).sub_node("window")
				.attribute("id").value(&win_id);

			if (win_id) {
				Nitpicker::Session_capability session_cap =
					nitpicker_root.lookup_nitpicker_session(win_id);

				focus_nitpicker_session.focus(session_cap);
			}

		} catch (...) {
			PWRN("no focus model available");
		}
	}

	Genode::Signal_rpc_member<Main> focus_dispatcher = { ep, *this, &Main::handle_focus_update };

	void handle_resize_request_update(unsigned)
	{
		try {
			resize_request_rom.update();

			char const * const node_type = "window";

			Xml_node window =
				Xml_node(resize_request_rom.local_addr<char>()).sub_node(node_type);

			for (;;) {
				unsigned long win_id = 0, width = 0, height = 0;

				window.attribute("id")    .value(&win_id);
				window.attribute("width") .value(&width);
				window.attribute("height").value(&height);

				nitpicker_root.request_resize(win_id, Area(width, height));

				if (window.is_last(node_type))
					break;

				window = window.next(node_type);
			}

		} catch (...) { /* no resize-request model available */ }
	}

	Genode::Signal_rpc_member<Main> resize_request_dispatcher =
		{ ep, *this, &Main::handle_resize_request_update };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		pointer_reporter.enabled(true);

		/* initially report an empty window list */
		window_list_reporter.enabled(true);
		Genode::Reporter::Xml_generator xml(window_list_reporter, [&] () { });

		focus_request_reporter.enabled(true);

		focus_rom.sigh(focus_dispatcher);
		resize_request_rom.sigh(resize_request_dispatcher);
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "desktop_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Wm::Main desktop(ep);
	}
}
