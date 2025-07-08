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

	void gen_start_node(Generator &g) const
	{
		if (!_nic.constructed())
			return;

		g.node("start", [&] {
			_nic->gen_start_node_content(g);
			gen_named_node(g, "binary", "nic");
			g.node("config", [&] { });
			g.node("route", [&] {
				gen_service_node<Platform::Session>(g, [&] {
					g.node("parent", [&] {
						g.attribute("label", "nic"); }); });
				gen_service_node<Uplink::Session>(g, [&] {
					g.node("child", [&] {
						g.attribute("name", "nic_router"); }); });
				gen_common_routes(g);
				gen_parent_rom_route(g, "nic");
				gen_parent_rom_route(g, "nic.dtb");
				gen_parent_route<Rm_session>(g);
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const use_nic = (board_info.detected.nic || board_info.soc.nic)
		                 &&  board_info.options.nic
		                 && !board_info.options.suspending;

		_nic.conditional(use_nic, registry, "nic", Priority::DEFAULT,
		                 Ram_quota { 20*1024*1024 }, Cap_quota { 300 });
	}
};

#endif /* _DRIVER__NIC_H_ */
