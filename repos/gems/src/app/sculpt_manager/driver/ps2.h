/*
 * \brief  Sculpt PS/2-driver management
 * \author Norman Feske
 * \date   2024-03-21
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__PS2_H_
#define _DRIVER__PS2_H_

namespace Sculpt { struct Ps2_driver; }


struct Sculpt::Ps2_driver : private Noncopyable
{
	Constructible<Child_state> _ps2 { };

	void gen_start_node(Xml_generator &xml) const
	{
		if (!_ps2.constructed())
			return;

		xml.node("start", [&] {
			_ps2->gen_start_node_content(xml);

			gen_named_node(xml, "binary", "ps2");

			xml.node("config", [&] {
				xml.attribute("capslock_led", "rom");
				xml.attribute("numlock_led",  "rom");
				xml.attribute("system",       "yes");
			});

			xml.node("route", [&] {
				gen_parent_route<Platform::Session>(xml);
				gen_common_routes(xml);
				gen_parent_rom_route(xml, "capslock", "capslock");
				gen_parent_rom_route(xml, "numlock",  "numlock");
				gen_parent_rom_route(xml, "system",   "config -> managed/system");
				gen_parent_route<Rom_session>   (xml);
				gen_service_node<Event::Session>(xml, [&] {
					xml.node("parent", [&] {
						xml.attribute("label", "ps2"); }); });
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const use_ps2 = board_info.detected.ps2
		                 && !board_info.options.suppress.ps2
		                 && !board_info.options.suspending;

		_ps2.conditional(use_ps2, registry, "ps2", Priority::MULTIMEDIA,
		                 Ram_quota { 1*1024*1024 }, Cap_quota { 100 });
	}
};

#endif /* _DRIVER__PS2_H_ */
