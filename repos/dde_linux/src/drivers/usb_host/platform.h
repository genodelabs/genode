/*
 * \brief  Platform specific definitions
 * \author Sebastian Sumpf
 * \date   2012-07-06
 *
 * These functions have to be implemented on all supported platforms.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <base/log.h>
#include <util/xml_node.h>
#include <irq_session/capability.h>

#include <lx_kit/env.h>

struct Services
{
	Genode::Env &env;

	/* report generation */
	bool raw_report_device_list = false;

	Services(Genode::Env &env) : env(env)
	{
		using namespace Genode;

		Genode::Xml_node config_node = Lx_kit::env().config_rom().xml();

		try {
			Genode::Xml_node node_report = config_node.sub_node("report");
			raw_report_device_list = node_report.attribute_value("devices", false);
		} catch (...) { }
	}
};

void backend_alloc_init(Genode::Env &env, Genode::Ram_session &ram, Genode::Allocator &alloc);

void platform_hcd_init(Services *services);
Genode::Irq_session_capability platform_irq_activate(int irq);

#endif /* _PLATFORM_H_ */
