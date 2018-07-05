/*
 * \brief  Menu view instance used for displaying the runtime view
 * \author Norman Feske
 * \date   2018-08-22
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
#include <runtime.h>


void Sculpt::gen_runtime_view_start_content(Xml_generator &xml, float font_size)
{
	gen_common_start_content(xml, "runtime_view", Cap_quota{200}, Ram_quota{9*1024*1024});

	gen_named_node(xml, "binary", "menu_view");

	xml.node("config", [&] () {
		xml.node("libc", [&] () { xml.attribute("stderr", "/dev/log"); });
		xml.node("report", [&] () { xml.attribute("hover", "yes"); });
		xml.node("vfs", [&] () {
			gen_named_node(xml, "tar", "menu_view_styles.tar");
			gen_named_node(xml, "rom", "Vera.ttf");
			gen_named_node(xml, "dir", "fonts", [&] () {
				gen_named_node(xml, "dir", "text", [&] () {
					gen_named_node(xml, "ttf", "regular", [&] () {
						xml.attribute("size_px", font_size);
						xml.attribute("cache", "256K");
						xml.attribute("path", "/Vera.ttf"); }); }); });

			gen_named_node(xml, "dir", "dev", [&] () {
				xml.node("log",  [&] () { }); });
		});
	});

	xml.node("route", [&] () {

		gen_service_node<Nitpicker::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", "leitzentrale -> runtime_view"); }); });

		gen_service_node<Rom_session>(xml, [&] () {
			xml.attribute("label", "dialog");
			xml.node("parent", [&] () { }); });

		gen_service_node<Report::Session>(xml, [&] () {
			xml.attribute("label", "hover");
			xml.node("parent", [&] () { }); });

		gen_parent_rom_route(xml, "menu_view");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "vfs_ttf.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "libpng.lib.so");
		gen_parent_rom_route(xml, "zlib.lib.so");
		gen_parent_rom_route(xml, "menu_view_styles.tar");
		gen_parent_rom_route(xml, "Vera.ttf");
		gen_parent_rom_route(xml, "dialog");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
	});
}

