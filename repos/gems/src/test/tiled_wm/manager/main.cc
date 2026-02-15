/*
 * \brief  Tiled-WM test: GUI manager
 * \author Christian Helmuth
 * \date   2018-09-26
 *
 * GUI manager implements the user-visible display state machine.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <util/string.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>


using Name = Genode::String<32>;


namespace Test { struct Manager; }

struct Test::Manager
{
	Genode::Env &env;

	Genode::Attached_rom_dataspace content_request_rom { env, "content_request" };
	Genode::Attached_rom_dataspace overlay_request_rom { env, "overlay_request" };

	Genode::Signal_handler<Manager> content_request_handler {
		env.ep(), *this, &Manager::handle_content_request };
	Genode::Signal_handler<Manager> overlay_request_handler {
		env.ep(), *this, &Manager::handle_overlay_request };

	Genode::Expanding_reporter apps_report    { env, "apps" };
	Genode::Expanding_reporter overlay_report { env, "overlay" };

	Genode::Expanding_reporter layout_rules_report { env, "rules", "layout_rules" };

	struct App {
		char const *label;
		char const *name;
		bool        visible;
	} apps[3] {
		{ "test-tiled_wm-app-1", "app1",     true },
		{ "test-tiled_wm-app-2", "app2",     false },
		{ "textedit",            "textedit", false }
	};

	bool overlay_visible { false };

	void report_apps();
	void report_overlay();
	void report_layout_rules();

	void handle_content_request();
	void handle_overlay_request();

	Manager(Genode::Env &env);
};


void Test::Manager::handle_content_request()
{
	content_request_rom.update();

	Name requested_app =
		content_request_rom.node().attribute_value("name", Name());

	if (!requested_app.valid()) return;

	App *found = nullptr;
	for (App &app : apps) {
		if (requested_app != app.name) continue;
		found = &app;
		break;
	}
	if (!found || found->visible) return;

	for (App &app : apps) {
		if (&app == found) {
			app.visible = true;
		} else {
			app.visible = false;
		}
	}

	report_apps();
	report_layout_rules();
}


void Test::Manager::handle_overlay_request()
{
	overlay_request_rom.update();

	bool const request_visible =
		overlay_request_rom.node().attribute_value("visible", false);

	if (request_visible == overlay_visible) return;

	overlay_visible = request_visible;

	report_overlay();
	report_layout_rules();
}


void Test::Manager::report_apps()
{
	apps_report.generate([&] (Genode::Generator &g) {
		for (App &app : apps) {
			g.node("app", [&] () {
				g.attribute("name", app.name);
				g.attribute("visible", app.visible);
			});
		}
	});
}


void Test::Manager::report_overlay()
{
	overlay_report.generate([&] (Genode::Generator &g) {
		g.attribute("visible", overlay_visible);
	});
}


void Test::Manager::report_layout_rules()
{
	layout_rules_report.generate([&] (Genode::Generator &g) {
		g.node("screen", [&] () {
			g.node("column", [&] () {
				g.attribute("name",  "screen");
				g.attribute("layer", "1");
				g.node("row", [&] () {
					g.attribute("name",   "panel");
					g.attribute("layer",  "2");
					g.attribute("height", "24");
				});
				g.node("row", [&] () {
					g.attribute("name", "content");
					g.attribute("layer", "4");
					g.node("column", [&] () {
						g.attribute("weight", "2");
					});
					g.node("column", [&] () {
						g.attribute("name",   "overlay");
						g.attribute("layer",  "3");
						g.attribute("weight", "1");
					});
				});
			});
		});
		g.node("assign", [&] () {
			g.attribute("label_prefix", "test-tiled_wm-panel");
			g.attribute("target", "panel");
		});
		g.node("assign", [&] () {
			g.attribute("label_prefix", "test-tiled_wm-overlay");
			g.attribute("target", "overlay");
			if (!overlay_visible)
				g.attribute("visible", false);
		});

		/* debug */
		if (false) {
			g.node("assign", [&] () {
				g.attribute("label_prefix", "");
				g.attribute("target", "screen");
				g.attribute("xpos", "any");
				g.attribute("ypos", "any");
			});
		}

		for (App &app : apps) {
			g.node("assign", [&] () {
				g.attribute("label_prefix", app.label);
				g.attribute("target", "content");
				if (!app.visible)
					g.attribute("visible", "false");
			});
		}
	});
}


Test::Manager::Manager(Genode::Env &env) : env(env)
{
	content_request_rom.sigh(content_request_handler);
	overlay_request_rom.sigh(overlay_request_handler);

	report_apps();
	report_overlay();
	report_layout_rules();
}


void Component::construct(Genode::Env &env) { static Test::Manager manager(env); }
