/*
 * \brief  XML configuration for spawning the administrative touch keyboard
 * \author Norman Feske
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RUNTIME__TOUCH_KEYBOARD_H_
#define _RUNTIME__TOUCH_KEYBOARD_H_

#include <runtime.h>

namespace Sculpt {

	enum class Alpha { OPAQUE, ALPHA };

	struct Touch_keyboard_attr
	{
		unsigned min_width, min_height;
		Alpha    alpha;
		Color    background;
	};

	static inline void gen_touch_keyboard(Generator &, Touch_keyboard_attr);
}


void Sculpt::gen_touch_keyboard(Generator &g, Touch_keyboard_attr const attr)
{
	g.node("start", [&] {

		gen_common_start_content(g, "manager_keyboard",
		                         Cap_quota{700}, Ram_quota{18*1024*1024},
		                         Priority::LEITZENTRALE);

		gen_named_node(g, "binary", "touch_keyboard", [&] { });

		g.node("config", [&] {
			g.attribute("min_width",  attr.min_width);
			g.attribute("min_height", attr.min_height);

			if (attr.alpha == Alpha::OPAQUE)
				g.attribute("opaque", "yes");

			g.attribute("background", String<20>(attr.background));
		});

		g.node("route", [&] {
			gen_parent_rom_route(g, "ld.lib.so");
			gen_parent_rom_route(g, "touch_keyboard");
			gen_parent_rom_route(g, "layout", "touch_keyboard_layout.config");
			gen_parent_rom_route(g, "menu_view");
			gen_parent_rom_route(g, "ld.lib.so");
			gen_parent_rom_route(g, "vfs.lib.so");
			gen_parent_rom_route(g, "libc.lib.so");
			gen_parent_rom_route(g, "libm.lib.so");
			gen_parent_rom_route(g, "libpng.lib.so");
			gen_parent_rom_route(g, "zlib.lib.so");
			gen_parent_rom_route(g, "sandbox.lib.so");
			gen_parent_rom_route(g, "dialog.lib.so");
			gen_parent_rom_route(g, "menu_view_styles.tar");

			gen_parent_route<Cpu_session>    (g);
			gen_parent_route<Pd_session>     (g);
			gen_parent_route<Log_session>    (g);
			gen_parent_route<Timer::Session> (g);

			gen_service_node<::File_system::Session>(g, [&] {
				g.attribute("label_prefix", "fonts ->");
				g.node("parent", [&] {
					g.attribute("identity", "leitzentrale -> fonts"); }); });

			gen_service_node<Gui::Session>(g, [&] {
				g.node("parent", [&] {
					g.attribute("label", "leitzentrale -> touch_keyboard"); }); });

			gen_service_node<Event::Session>(g, [&] {
				g.node("parent", [&] {
					g.attribute("label", "global"); }); });
		});
	});
}

#endif /* _RUNTIME__TOUCH_KEYBOARD_H_ */
