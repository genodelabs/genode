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
#include <os/config.h>

using namespace Genode;

int main(void)
{
	try {
		config()->xml_node().sub_node("test_config_subnode");
	} catch (Xml_node::Nonexistent_sub_node) {
		error("missing '<test_config_subnode>' sub node");
		return -1;
	}

	log("Test succeeded");

	return 0;
}
