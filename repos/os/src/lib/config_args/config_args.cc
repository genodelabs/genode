/*
 * \brief   Read program arguments from the config file
 * \date    2012-04-19
 * \author  Christian Prochaska
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/config.h>
#include <base/log.h>

using namespace Genode;


/* external symbols provided by Genode's startup code */
extern char **genode_argv;
extern int    genode_argc;


__attribute__((constructor))
void init_config_args(void)
{
	int argc = 0;
	static char **argv;

	/* count the number of arguments */
	try {
		Xml_node arg_node = config()->xml_node().sub_node("arg");
		for (;;) {
			/* check if the 'value' attribute exists */
			arg_node.attribute("value");
			argc++;
			arg_node = arg_node.next("arg");
		}
	}
	catch (Xml_node::Nonexistent_sub_node) { }
	catch (Xml_node::Nonexistent_attribute)
	{
		error("<arg> node has no 'value' attribute, ignoring further <arg> nodes");
	}

	if (argc == 0)
		return;

	argv = (char**)env()->heap()->alloc((argc + 1) * sizeof(char*));

	/* read the arguments */
	Xml_node arg_node = config()->xml_node().sub_node("arg");
	try {
		for (int i = 0; i < argc; i++) {
			static char buf[512];
			arg_node.attribute("value").value(buf, sizeof(buf));
			size_t arg_size = strlen(buf) + 1;
			argv[i] = (char*)env()->heap()->alloc(arg_size);
			strncpy(argv[i], buf, arg_size);
			arg_node = arg_node.next("arg");
		}
	} catch (Xml_node::Nonexistent_sub_node) { }

	argv[argc] = 0;

	/* register command-line arguments at Genode's startup code */
	genode_argc = argc;
	genode_argv = argv;
}
