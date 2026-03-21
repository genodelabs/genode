/*
 * \brief  Sculpt NIC-driver management
 * \author Norman Feske
 * \date   2024-03-26
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__NIC_H_
#define _DRIVER__NIC_H_

namespace Sculpt { struct Nic_driver; }


struct Sculpt::Nic_driver : private Noncopyable
{
	Constructible<Child_state> _nic { };

	void gen_child_node(Generator &g) const
	{
		if (!_nic.constructed())
			return;

		g.node("child", [&] {
			_nic->gen_child_node_content(g);
			g.node("config", [&] { });
			g.tabular_node("connect", [&] {
				connect_platform(g, "nic");
				g.node("uplink", [&] {
					g.node("child", [&] {
						g.attribute("name", "nic_router"); }); });
				connect_report(g);
				connect_parent_rom(g, "nic.dtb");
				g.node("rm",  [&] { g.node("parent"); });
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const use_nic = (board_info.detected.nic || board_info.soc.nic)
		                 &&  board_info.options.nic
		                 && !board_info.options.suspending;

		_nic.conditional(use_nic, registry, Priority::DEFAULT,
		                 Child_name { "nic" }, Binary_name { "nic" },
		                 Ram_quota { 20*1024*1024 }, Cap_quota { 300 });
	}
};

#endif /* _DRIVER__NIC_H_ */
