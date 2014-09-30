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
#include <menu_dialog.h>


namespace Launcher { struct Main; }

struct Launcher::Main
{
	Server::Entrypoint ep;

	Genode::Cap_connection cap;

	char const *report_rom_config =
		"<config> <rom>"
		"  <policy label=\"menu_dialog\"    report=\"menu_dialog\"/>"
		"  <policy label=\"menu_hover\"     report=\"menu_hover\"/>"
		"  <policy label=\"context_dialog\" report=\"context_dialog\"/>"
		"  <policy label=\"context_hover\"  report=\"context_hover\"/>"
		"</rom> </config>";

	Report_rom_slave report_rom_slave = { cap, *env()->ram_session(), report_rom_config };


	/**
	 * Nitpicker session used to perform session-control operations on the
	 * subsystem's nitpicker sessions.
	 */
	Nitpicker::Connection nitpicker;

	Subsystem_manager subsystem_manager { ep, cap };

	Menu_dialog menu_dialog { ep, cap, *env()->ram_session(), report_rom_slave,
	                          subsystem_manager, nitpicker };


	Lazy_volatile_object<Attached_rom_dataspace> xray_rom_ds;

	enum Visibility { VISIBILITY_ALWAYS, VISIBILITY_XRAY };

	Visibility visibility = VISIBILITY_ALWAYS;

	void handle_config(unsigned);

	Genode::Signal_rpc_member<Main> xray_update_dispatcher =
		{ ep, *this, &Main::handle_xray_update };

	void handle_xray_update(unsigned);


	/**
	 * Constructor
	 */
	Main(Server::Entrypoint &ep) : ep(ep)
	{
		handle_config(0);

		if (visibility == VISIBILITY_ALWAYS)
			menu_dialog.visible(true);
	}
};


void Launcher::Main::handle_config(unsigned)
{
	config()->reload();

	/* set default visibility */
	visibility = VISIBILITY_ALWAYS;

	/* obtain model about nitpicker's xray mode */
	if (config()->xml_node().has_attribute("visibility")) {
		if (config()->xml_node().attribute("visibility").has_value("xray")) {
			xray_rom_ds.construct("xray");
			xray_rom_ds->sigh(xray_update_dispatcher);

			visibility = VISIBILITY_XRAY;

			/* manually import the initial xray state */
			handle_xray_update(0);
		}
	}

	menu_dialog.update();
}


void Launcher::Main::handle_xray_update(unsigned)
{
	xray_rom_ds->update();
	if (!xray_rom_ds->is_valid()) {
		PWRN("could not access xray info");
		menu_dialog.visible(false);
		return;
	}

	Xml_node xray(xray_rom_ds->local_addr<char>());

	bool const visible = xray.has_attribute("enabled")
	                  && xray.attribute("enabled").has_value("yes");

	menu_dialog.visible(visible);
}


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "desktop_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		/* look for dynamic linker */
		try {
			static Rom_connection rom("ld.lib.so");
			Process::dynamic_linker(rom.dataspace());
		} catch (...) { }

		static Launcher::Main desktop(ep);
	}
}
