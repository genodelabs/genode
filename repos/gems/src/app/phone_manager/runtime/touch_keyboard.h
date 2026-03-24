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
	g.node("child", [&] {

		gen_child_attr(g, Child_name { "manager_keyboard" }, Binary_name { "touch_keyboard" },
		               Cap_quota{700}, Ram_quota{18*1024*1024}, Priority::LEITZENTRALE);

		g.node("config", [&] {
			g.attribute("min_width",  attr.min_width);
			g.attribute("min_height", attr.min_height);

			if (attr.alpha == Alpha::OPAQUE)
				g.attribute("opaque", "yes");

			g.attribute("background", String<20>(attr.background));
		});

		g.node("connect", [&] {
			connect_parent_rom(g, "ld.lib.so");
			connect_parent_rom(g, "touch_keyboard");
			connect_parent_rom(g, "layout", "touch_keyboard_layout.config");
			connect_parent_rom(g, "menu_view");
			connect_parent_rom(g, "ld.lib.so");
			connect_parent_rom(g, "vfs.lib.so");
			connect_parent_rom(g, "libc.lib.so");
			connect_parent_rom(g, "libm.lib.so");
			connect_parent_rom(g, "libpng.lib.so");
			connect_parent_rom(g, "zlib.lib.so");
			connect_parent_rom(g, "sandbox.lib.so");
			connect_parent_rom(g, "dialog.lib.so");
			connect_parent_rom(g, "menu_view_style.tar");

			gen_named_node(g, "fs", "font", [&] {
				gen_named_node(g, "child", "font"); });

			g.node("gui", [&] {
				gen_named_node(g, "child", "leitzentrale", [&] {
					g.attribute("label", "touch_keyboard"); }); });

			connect_event(g, "global");
		});
	});
}

#endif /* _RUNTIME__TOUCH_KEYBOARD_H_ */
