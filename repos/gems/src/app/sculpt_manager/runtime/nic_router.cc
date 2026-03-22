/*
 * \brief  Configuration for the NIC router
 * \author Norman Feske
 * \date   2018-05-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <uplink_session/uplink_session.h>

/* local includes */
#include <runtime.h>


void Sculpt::gen_nic_router_child_content(Generator &g)
{
	gen_child_attr(g, Child_name { "nic_router" }, Binary_name { "nic_router" },
	               Cap_quota{300}, Ram_quota{10*1024*1024},
	               Priority::NETWORK);

	g.node("provides", [&] {
		g.node("nic");
		g.node("uplink");
	});

	g.tabular_node("connect", [&] {
		connect_config_rom(g, "config", "child/nic_router");
		connect_report(g);
	});
}
