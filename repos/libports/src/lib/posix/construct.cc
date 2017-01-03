/*
 * \brief  Entry point for POSIX applications
 * \author Norman Feske
 * \date   2016-12-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <libc/component.h>

/* libc includes */
#include <stdlib.h> /* 'malloc' */
#include <stdlib.h> /* 'exit'   */

extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;


/* provided by the application */
extern "C" int main(int argc, char ** argv, char **envp);


void Libc::Component::construct(Libc::Env &env)
{
	using Genode::Xml_node;
	using Genode::Xml_attribute;

	env.config([&] (Xml_node const &node) {
		int argc = 0;
		static char **argv;

		/* count the number of arguments */
		node.for_each_sub_node("arg", [&] (Xml_node const &arg_node) {
			/* check if the 'value' attribute exists */
			if (arg_node.has_attribute("value"))
				++argc;
		});

		if (argc == 0)
			return;

		argv = (char**)malloc((argc + 1) * sizeof(char*));

		/* read the arguments */
		int i = 0;
		node.for_each_sub_node("arg", [&] (Xml_node const &arg_node) {
			try {
				Xml_attribute attr = arg_node.attribute("value");
				Genode::size_t const arg_len = attr.value_size()+1;
				argv[i] = (char*)malloc(arg_len);
				attr.value(argv[i], arg_len);
				++i;
			} catch (Xml_node::Nonexistent_sub_node) { }
		});

		argv[i] = nullptr;

		/* register command-line arguments at Genode's startup code */
		genode_argc = argc;
		genode_argv = argv;
	});

	/* TODO: environment variables, see #2236 */

	exit(main(genode_argc, genode_argv, genode_envp));
}
