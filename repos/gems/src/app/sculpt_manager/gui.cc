/*
 * \brief  Sculpt GUI management
 * \author Norman Feske
 * \date   2018-04-30
 *
 * The GUI is based on a dynamically configured init component, which hosts
 * one menu-view component for each dialog.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <gui.h>


void Sculpt::Gui::_gen_menu_view_start_content(Xml_generator &xml,
                                               Label const &label,
                                               Point pos) const
{
	xml.attribute("version", version.value);

	gen_common_start_content(xml, label, Cap_quota{150}, Ram_quota{8*1024*1024});

	gen_named_node(xml, "binary", "menu_view");

	xml.node("config", [&] () {
		xml.attribute("xpos", pos.x());
		xml.attribute("ypos", pos.y());
		xml.attribute("width", menu_width);
		xml.node("libc", [&] () { xml.attribute("stderr", "/dev/log"); });
		xml.node("report", [&] () { xml.attribute("hover", "yes"); });
		xml.node("vfs", [&] () {
			gen_named_node(xml, "tar", "menu_view_styles.tar");
			gen_named_node(xml, "dir", "styles", [&] () {
				gen_named_node(xml, "dir", "frame", [&] () {
					gen_named_node(xml, "dir", "logo", [&] () {
						gen_named_node(xml, "rom", "background.png", [&] () {
							xml.attribute("label", "genode_logo.png"); }); }); }); });

			gen_named_node(xml, "dir", "fonts", [&] () {
				xml.node("fs", [&] () {
					xml.attribute("label", "fonts"); }); });
			gen_named_node(xml, "dir", "dev", [&] () {
				xml.node("log",  [&] () { }); });
		});
	});

	xml.node("route", [&] () {
		gen_parent_rom_route(xml, "menu_view");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "libpng.lib.so");
		gen_parent_rom_route(xml, "zlib.lib.so");
		gen_parent_rom_route(xml, "menu_view_styles.tar");
		gen_parent_rom_route(xml, "genode_logo.png");
		gen_parent_route<Cpu_session>         (xml);
		gen_parent_route<Pd_session>          (xml);
		gen_parent_route<Log_session>         (xml);
		gen_parent_route<Timer::Session>      (xml);
		gen_parent_route<Nitpicker::Session>  (xml);

		gen_service_node<Rom_session>(xml, [&] () {
			xml.attribute("label", "dialog");
			xml.node("parent", [&] () { }); });

		gen_service_node<Report::Session>(xml, [&] () {
			xml.attribute("label", "hover");
			xml.node("parent", [&] () { }); });

		gen_service_node<::File_system::Session>(xml, [&] () {
			xml.attribute("label", "fonts");
			xml.node("parent", [&] () {
				xml.attribute("label", "fonts"); }); });
	});
}


void Sculpt::Gui::_generate_config(Xml_generator &xml) const
{
	xml.node("parent-provides", [&] () {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Report::Session>(xml);
		gen_parent_service<Nitpicker::Session>(xml);
		gen_parent_service<::File_system::Session>(xml);
	});

	xml.node("start", [&] () {
		_gen_menu_view_start_content(xml, "menu_view", Point(0, 0)); });
}

