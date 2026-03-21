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

	void gen_child_node(Generator &g) const
	{
		if (!_ps2.constructed())
			return;

		g.node("child", [&] {
			_ps2->gen_child_node_content(g);

			g.node("config", [&] {
				g.attribute("capslock_led", "rom");
				g.attribute("numlock_led",  "rom");
				g.attribute("system",       "yes");
			});

			g.tabular_node("connect", [&] {
				connect_platform(g, "ps2");
				connect_report_rom(g, "capslock", "global_keys/capslock");
				connect_report_rom(g, "numlock",  "global_keys/numlock");
				connect_config_rom(g, "system", "system");
				connect_event(g, "ps2");
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const use_ps2 = board_info.detected.ps2
		                 && !board_info.options.suppress.ps2
		                 && !board_info.options.suspending;

		_ps2.conditional(use_ps2, registry, Priority::MULTIMEDIA,
		                 Child_name { "ps2" }, Binary_name { "ps2" },
		                 Ram_quota { 1*1024*1024 }, Cap_quota { 100 });
	}
};

#endif /* _DRIVER__PS2_H_ */
