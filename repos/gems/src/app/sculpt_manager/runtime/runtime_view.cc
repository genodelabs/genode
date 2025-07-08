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


void Sculpt::gen_runtime_view_start_content(Generator &g,
                                            Child_state const &state,
                                            double font_size)
{
	state.gen_start_node_content(g);

	gen_named_node(g, "resource", "CPU", [&] {
		g.attribute("quantum", 20); });

	gen_named_node(g, "binary", "menu_view");

	g.node("config", [&] {
		g.node("libc", [&] { g.attribute("stderr", "/dev/log"); });
		g.node("report", [&] { g.attribute("hover", "yes"); });
		g.node("vfs", [&] {
			gen_named_node(g, "tar", "menu_view_styles.tar");
			gen_named_node(g, "rom", "Vera.ttf");
			gen_named_node(g, "dir", "fonts", [&] {
				gen_named_node(g, "dir", "text", [&] {
					gen_named_node(g, "ttf", "regular", [&] {
						g.attribute("size_px", font_size);
						g.attribute("cache", "256K");
						g.attribute("path", "/Vera.ttf"); }); }); });

			gen_named_node(g, "dir", "dev", [&] {
				g.node("log",  [&] { }); });
		});
	});

	g.node("route", [&] {

		gen_service_node<Gui::Session>(g, [&] {
			g.node("parent", [&] {
				g.attribute("label", "leitzentrale -> runtime_view"); }); });

		gen_service_node<Rom_session>(g, [&] {
			g.attribute("label", "dialog");
			g.node("parent", [&] {
				g.attribute("label", "leitzentrale -> runtime_view -> dialog"); }); });

		gen_service_node<Report::Session>(g, [&] {
			g.attribute("label", "hover");
			g.node("parent", [&] {
				g.attribute("label", "leitzentrale -> runtime_view -> hover"); }); });

		gen_parent_rom_route(g, "menu_view");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_rom_route(g, "vfs_ttf.lib.so");
		gen_parent_rom_route(g, "libc.lib.so");
		gen_parent_rom_route(g, "libm.lib.so");
		gen_parent_rom_route(g, "libpng.lib.so");
		gen_parent_rom_route(g, "zlib.lib.so");
		gen_parent_rom_route(g, "menu_view_styles.tar");
		gen_parent_rom_route(g, "Vera.ttf");
		gen_parent_rom_route(g, "dialog");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Timer::Session> (g);
	});
}

