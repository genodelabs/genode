/*
 * \brief  GDB Monitor target config test
 * \author Christian Prochaska
 * \date   2012-04-16
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>

using namespace Genode;

int main(void)
{
	try {
		config()->xml_node().sub_node("test_config_subnode");
	} catch(Config::Invalid) {
		PERR("Error: Missing '<config>' node.");
		return -1;
	} catch (Xml_node::Nonexistent_sub_node) {
		PERR("Error: Missing '<test_config_subnode>' sub node.");
		return -1;
	}

	printf("Test succeeded\n");

	return 0;
}
