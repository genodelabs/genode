/*
 * \brief  Sculpt touchscreen-driver management
 * \author Norman Feske
 * \date   2024-03-25
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__TOUCH_H_
#define _DRIVER__TOUCH_H_

namespace Sculpt { struct Touch_driver; }


struct Sculpt::Touch_driver : private Noncopyable
{
	Constructible<Child_state> _soc { };

	void gen_start_node(Generator &g) const
	{
		if (!_soc.constructed())
			return;

		g.node("start", [&] {
			_soc->gen_start_node_content(g);
			gen_named_node(g, "binary", "touch");
			g.node("config", [&] { });
			g.tabular_node("route", [&] {
				gen_parent_route<Platform::Session>   (g);
				gen_parent_rom_route(g, "dtb", "touch.dtb");
				gen_parent_rom_route(g, "touch");
				gen_common_routes(g);
				gen_parent_route<Pin_control::Session>(g);
				gen_parent_route<Irq_session>         (g);
				gen_service_node<Event::Session>(g, [&] {
					g.node("parent", [&] {
						g.attribute("label", "touch"); }); });
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		_soc.conditional(board_info.soc.touch && board_info.options.display,
		                 registry, Child_state::Attr {
		                    .name      = "touch",
		                    .priority  = Priority::MULTIMEDIA,
		                    .cpu_quota = 10,
		                    .location  = { },
		                    .initial   = { Ram_quota { 10*1024*1024 },
		                                   Cap_quota { 250 } },
		                    .max       = { } } );
	}
};

#endif /* _DRIVER__TOUCH_H_ */
