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

	static inline void gen_touch_keyboard(Xml_generator &, Touch_keyboard_attr);
}


void Sculpt::gen_touch_keyboard(Xml_generator &xml, Touch_keyboard_attr const attr)
{
	xml.node("start", [&] {

		gen_common_start_content(xml, "manager_keyboard",
		                         Cap_quota{700}, Ram_quota{18*1024*1024},
		                         Priority::LEITZENTRALE);

		gen_named_node(xml, "binary", "touch_keyboard", [&] { });

		xml.node("config", [&] {
			xml.attribute("min_width",  attr.min_width);
			xml.attribute("min_height", attr.min_height);

			if (attr.alpha == Alpha::OPAQUE)
				xml.attribute("opaque", "yes");

			xml.attribute("background", String<20>(attr.background));
		});

		xml.node("route", [&] {
			gen_parent_rom_route(xml, "ld.lib.so");
			gen_parent_rom_route(xml, "touch_keyboard");
			gen_parent_rom_route(xml, "layout", "touch_keyboard_layout.config");
			gen_parent_rom_route(xml, "menu_view");
			gen_parent_rom_route(xml, "ld.lib.so");
			gen_parent_rom_route(xml, "vfs.lib.so");
			gen_parent_rom_route(xml, "libc.lib.so");
			gen_parent_rom_route(xml, "libm.lib.so");
			gen_parent_rom_route(xml, "libpng.lib.so");
			gen_parent_rom_route(xml, "zlib.lib.so");
			gen_parent_rom_route(xml, "sandbox.lib.so");
			gen_parent_rom_route(xml, "menu_view_styles.tar");

			gen_parent_route<Cpu_session>    (xml);
			gen_parent_route<Pd_session>     (xml);
			gen_parent_route<Log_session>    (xml);
			gen_parent_route<Timer::Session> (xml);

			gen_service_node<::File_system::Session>(xml, [&] {
				xml.attribute("label", "fonts");
				xml.node("parent", [&] {
					xml.attribute("label", "leitzentrale -> fonts"); }); });

			gen_service_node<Gui::Session>(xml, [&] {
				xml.node("parent", [&] {
					xml.attribute("label", "leitzentrale -> touch_keyboard"); }); });

			gen_service_node<Event::Session>(xml, [&] {
				xml.node("parent", [&] {
					xml.attribute("label", "global"); }); });
		});
	});
}

#endif /* _RUNTIME__TOUCH_KEYBOARD_H_ */
