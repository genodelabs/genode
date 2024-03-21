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


typedef Genode::String<32> Name;


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

	Genode::Reporter apps_report         { env, "apps" };
	Genode::Reporter overlay_report      { env, "overlay" };

	Genode::Reporter layout_rules_report { env, "rules", "layout_rules" };

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
		content_request_rom.xml().attribute_value("name", Name());

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
		overlay_request_rom.xml().attribute_value("visible", false);

	if (request_visible == overlay_visible) return;

	overlay_visible = request_visible;

	report_overlay();
	report_layout_rules();
}


void Test::Manager::report_apps()
{
	Genode::Reporter::Xml_generator xml(apps_report, [&] () {
		for (App &app : apps) {
			xml.node("app", [&] () {
				xml.attribute("name", app.name);
				xml.attribute("visible", app.visible);
			});
		}
	});
}


void Test::Manager::report_overlay()
{
	Genode::Reporter::Xml_generator xml(overlay_report, [&] () {
		xml.attribute("visible", overlay_visible);
	});
}


void Test::Manager::report_layout_rules()
{
	Genode::Reporter::Xml_generator xml(layout_rules_report, [&] () {
		xml.node("screen", [&] () {
			xml.node("column", [&] () {
				xml.attribute("name",  "screen");
				xml.attribute("layer", "1");
				xml.node("row", [&] () {
					xml.attribute("name",   "panel");
					xml.attribute("layer",  "2");
					xml.attribute("height", "24");
				});
				xml.node("row", [&] () {
					xml.attribute("name", "content");
					xml.attribute("layer", "4");
					xml.node("column", [&] () {
						xml.attribute("weight", "2");
					});
					xml.node("column", [&] () {
						xml.attribute("name",   "overlay");
						xml.attribute("layer",  "3");
						xml.attribute("weight", "1");
					});
				});
			});
		});
		xml.node("assign", [&] () {
			xml.attribute("label_prefix", "test-tiled_wm-panel");
			xml.attribute("target", "panel");
		});
		xml.node("assign", [&] () {
			xml.attribute("label_prefix", "test-tiled_wm-overlay");
			xml.attribute("target", "overlay");
			if (!overlay_visible)
				xml.attribute("visible", false);
		});

		/* debug */
		if (false) {
			xml.node("assign", [&] () {
				xml.attribute("label_prefix", "");
				xml.attribute("target", "screen");
				xml.attribute("xpos", "any");
				xml.attribute("ypos", "any");
			});
		}

		for (App &app : apps) {
			xml.node("assign", [&] () {
				xml.attribute("label_prefix", app.label);
				xml.attribute("target", "content");
				if (!app.visible)
					xml.attribute("visible", "false");
			});
		}
	});
}


Test::Manager::Manager(Genode::Env &env) : env(env)
{
	apps_report.enabled(true);
	overlay_report.enabled(true);
	layout_rules_report.enabled(true);

	content_request_rom.sigh(content_request_handler);
	overlay_request_rom.sigh(overlay_request_handler);

	report_apps();
	report_overlay();
	report_layout_rules();
}


void Component::construct(Genode::Env &env) { static Test::Manager manager(env); }
