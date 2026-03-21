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

	void gen_child_node(Generator &g) const
	{
		if (!_soc.constructed())
			return;

		g.node("child", [&] {
			_soc->gen_child_node_content(g);
			g.node("config", [&] { });
			g.tabular_node("connect", [&] {
				connect_platform(g, "touch");
				connect_parent_rom(g, "dtb", "touch.dtb");
				connect_pin_control(g, "PH4",  "touch-int");   /* pinephone */
				connect_pin_control(g, "PH11", "touch-reset"); /* pinephone */
				g.node("irq", [&] {                            /* pinephone */
					gen_named_node(g, "child", "platform", [&] {
						g.attribute("label", "touch-int"); }); });
				connect_event(g, "touch");
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		_soc.conditional(board_info.soc.touch && board_info.options.display,
		                 registry, Child_state::Attr {
		                    .name      = "touch",
		                    .binary    = "touch",
		                    .priority  = Priority::MULTIMEDIA,
		                    .location  = { },
		                    .initial   = { Ram_quota { 10*1024*1024 },
		                                   Cap_quota { 250 } },
		                    .max       = { } } );
	}
};

#endif /* _DRIVER__TOUCH_H_ */
