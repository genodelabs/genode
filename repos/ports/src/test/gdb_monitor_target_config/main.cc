/*
 * \brief  GDB Monitor target config test
 * \author Christian Prochaska
 * \date   2012-04-16
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>

void Component::construct(Genode::Env & env)
{
	Genode::Attached_rom_dataspace config(env, "config");
	try {
		config.xml().sub_node("test_config_subnode");
		Genode::log("Test succeeded");
	} catch (Genode::Xml_node::Nonexistent_sub_node) {
		Genode::error("missing '<test_config_subnode>' sub node");
	}
}
