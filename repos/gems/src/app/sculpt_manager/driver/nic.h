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

	void gen_start_node(Xml_generator &xml) const
	{
		if (!_nic.constructed())
			return;

		xml.node("start", [&] {
			_nic->gen_start_node_content(xml);
			gen_named_node(xml, "binary", "nic");
			xml.node("config", [&] { });
			xml.node("route", [&] {
				gen_service_node<Platform::Session>(xml, [&] {
					xml.node("parent", [&] {
						xml.attribute("label", "nic"); }); });
				gen_service_node<Uplink::Session>(xml, [&] {
					xml.node("child", [&] {
						xml.attribute("name", "nic_router"); }); });
				gen_common_routes(xml);
				gen_parent_rom_route(xml, "nic");
				gen_parent_rom_route(xml, "nic.dtb");
				gen_parent_route<Rm_session>(xml);
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
