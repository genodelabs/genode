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
#include <os/attached_dataspace.h>
#include <util/volatile_object.h>
#include <util/xml_node.h>

/* gems includes */
#include <gems/single_session_service.h>
#include <gems/report_rom_slave.h>
#include <gems/local_reporter.h>

/* local includes */
#include <decorator_nitpicker.h>
#include <decorator_slave.h>
#include <window_layouter_slave.h>
#include <nitpicker.h>

namespace Wm {

	class Main;

	using Genode::size_t;
	using Genode::env;
	using Genode::Rom_session_client;
	using Genode::Rom_connection;
	using Genode::Xml_node;
}


struct Wm::Main
{
	Server::Entrypoint ep;

	Genode::Cap_connection cap;

	char const *report_rom_config =
		"<config> <rom>"
		"  <policy label=\"window_list\"    report=\"window_list\"/>"
		"  <policy label=\"window_layout\"  report=\"window_layout\"/>"
		"  <policy label=\"resize_request\" report=\"resize_request\"/>"
		"  <policy label=\"pointer\"        report=\"pointer\"/>"
		"  <policy label=\"hover\"          report=\"hover\"/>"
		"  <policy label=\"focus\"          report=\"focus\"/>"
		"</rom> </config>";

	Report_rom_slave report_rom_slave = { cap, *env()->ram_session(), report_rom_config };

	Rom_session_capability window_list_rom    = report_rom_slave.rom_session("window_list");
	Rom_session_capability window_layout_rom  = report_rom_slave.rom_session("window_layout");
	Rom_session_capability pointer_rom        = report_rom_slave.rom_session("pointer");
	Rom_session_capability hover_rom          = report_rom_slave.rom_session("hover");

	Rom_session_client focus_rom          { report_rom_slave.rom_session("focus") };
	Rom_session_client resize_request_rom { report_rom_slave.rom_session("resize_request") };

	/* pointer position reported by nitpicker */
	Capability<Report::Session> pointer_report = report_rom_slave.report_session("pointer");
	Local_reporter pointer_reporter = { "pointer", pointer_report };

	/* hovered element reported by decorator */
	Capability<Report::Session> hover_report = report_rom_slave.report_session("hover");

	Capability<Report::Session> window_list_report = report_rom_slave.report_session("window_list");
	Local_reporter window_list_reporter = { "window_list", window_list_report };

	Capability<Report::Session> window_layout_report  = report_rom_slave.report_session("window_layout");
	Capability<Report::Session> resize_request_report = report_rom_slave.report_session("resize_request");
	Capability<Report::Session> focus_report          = report_rom_slave.report_session("focus");

	Input::Session_component window_layouter_input;

	/* handler that forwards clicks into unfocused windows to the layouter */
	struct Click_handler : Nitpicker::Click_handler
	{
		Input::Session_component &window_layouter_input;
		Local_reporter           &pointer_reporter;

		void _submit_button_event(Input::Event::Type type, Nitpicker::Point pos)
		{
			window_layouter_input.submit(Input::Event(type, Input::BTN_LEFT,
			                                          pos.x(), pos.y(), 0, 0));
		}

		void handle_enter(Nitpicker::Point pos) override
		{
			Local_reporter::Xml_generator xml(pointer_reporter, [&] ()
			{
				xml.attribute("xpos", pos.x());
				xml.attribute("ypos", pos.y());
			});
		}

		void handle_click(Nitpicker::Point pos) override
		{
			/*
			 * Propagate clicked-at position to decorator such that it can
			 * update its hover model.
			 */
			Local_reporter::Xml_generator xml(pointer_reporter, [&] ()
			{
				xml.attribute("xpos", pos.x());
				xml.attribute("ypos", pos.y());
			});

			/*
			 * Supply artificial mouse click to the decorator's input session
			 * (which is routed to the layouter).
			 */
			_submit_button_event(Input::Event::PRESS,   pos);
			_submit_button_event(Input::Event::RELEASE, pos);
		}

		Click_handler(Input::Session_component &window_layouter_input,
		              Local_reporter           &pointer_reporter)
		:
			window_layouter_input(window_layouter_input),
			pointer_reporter(pointer_reporter)
		{ }

	} click_handler { window_layouter_input, pointer_reporter };

	Window_registry window_registry { *env()->heap(), window_list_reporter };

	Nitpicker::Root nitpicker_root { ep, window_registry,
	                                 *env()->heap(), env()->ram_session_cap(),
	                                 click_handler };

	Decorator_nitpicker_service decorator_nitpicker_service {
		ep, *env()->heap(), env()->ram_session_cap(), pointer_reporter,
		window_layouter_input, nitpicker_root };

	Window_layouter_slave window_layouter_slave = {
		cap, *env()->ram_session(), window_list_rom, hover_rom,
		ep.manage(window_layouter_input), window_layout_report,
		resize_request_report, focus_report };

	Decorator_slave decorator_slave = {
		cap, decorator_nitpicker_service, *env()->ram_session(),
		window_layout_rom, pointer_rom, hover_report };

	Genode::Lazy_volatile_object<Attached_dataspace> focus_ds;

	Nitpicker::Connection focus_nitpicker_session;

	void handle_focus_update(unsigned)
	{
		try {
			if (!focus_ds.is_constructed() || focus_rom.update() == false) {
				if (focus_ds.is_constructed())
					focus_ds->invalidate();
				focus_ds.construct(focus_rom.dataspace());
			}

			unsigned long win_id = 0;

			Xml_node(focus_ds->local_addr<char>()).sub_node("window")
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

	Genode::Lazy_volatile_object<Attached_dataspace> resize_request_ds;

	void handle_resize_request_update(unsigned)
	{
		try {
			if (!resize_request_ds.is_constructed() || resize_request_rom.update() == false) {
				if (resize_request_ds.is_constructed())
					resize_request_ds->invalidate();
				resize_request_ds.construct(resize_request_rom.dataspace());
			}

			char const * const node_type = "window";

			Xml_node window =
				Xml_node(resize_request_ds->local_addr<char>()).sub_node(node_type);

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
		window_layouter_input.event_queue().enabled(true);

		/* initially report an empty window list */
		Local_reporter::Xml_generator xml(window_list_reporter, [&] () { });

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
