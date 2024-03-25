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

#ifndef _TOUCH_DRIVER_H_
#define _TOUCH_DRIVER_H_

/* local includes */
#include <model/child_exit_state.h>
#include <model/board_info.h>
#include <runtime.h>

namespace Sculpt { struct Touch_driver; }


struct Sculpt::Touch_driver : private Noncopyable
{
	Constructible<Child_state> _soc { };

	void gen_start_node(Xml_generator &xml) const
	{
		if (!_soc.constructed())
			return;

		xml.node("start", [&] {
			_soc->gen_start_node_content(xml);
			gen_named_node(xml, "binary", "touch_drv");
			xml.node("config", [&] { });
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>   (xml);
				gen_parent_rom_route(xml, "dtb", "touch_drv.dtb");
				gen_parent_rom_route(xml, "ld.lib.so");
				gen_parent_rom_route(xml, "touch_drv");
				gen_parent_route<Pin_control::Session>(xml);
				gen_parent_route<Irq_session>         (xml);
				gen_parent_route<Cpu_session>         (xml);
				gen_parent_route<Pd_session>          (xml);
				gen_parent_route<Log_session>         (xml);
				gen_parent_route<Timer::Session>      (xml);
				gen_service_node<Event::Session>(xml, [&] {
					xml.node("parent", [&] {
						xml.attribute("label", "touch"); }); });
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		_soc.conditional(board_info.soc_touch_present,
		                 registry, "touch", Priority::MULTIMEDIA,
		                 Ram_quota { 10*1024*1024 }, Cap_quota { 250 });
	}
};

#endif /* _TOUCH_DRIVER_H_ */
