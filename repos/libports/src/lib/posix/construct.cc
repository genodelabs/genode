/*
 * \brief  Entry point for POSIX applications
 * \author Norman Feske
 * \date   2016-12-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>

/* libc includes */
#include <stdlib.h> /* 'malloc' */
#include <stdlib.h> /* 'exit'   */

extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;

/* initial environment for the FreeBSD libc implementation */
extern char **environ;

/* provided by the application */
extern "C" int main(int argc, char ** argv, char **envp);


static void construct_component(Libc::Env &env)
{
	using Genode::Xml_node;
	using Genode::Xml_attribute;

	env.config([&] (Xml_node const &node) {
		int argc = 0;
		int envc = 0;
		char **argv;
		char **envp;

		/* count the number of arguments and environment variables */
		node.for_each_sub_node([&] (Xml_node const &node) {
			/* check if the 'value' attribute exists */
			if (node.has_type("arg") && node.has_attribute("value"))
				++argc;
			else
			if (node.has_type("env") && node.has_attribute("key") && node.has_attribute("value"))
				++envc;
		});

		if (argc == 0 && envc == 0)
			return; /* from lambda */

		/* arguments and environment are a contiguous array (but don't count on it) */
		argv = (char**)malloc((argc + envc + 1) * sizeof(char*));
		envp = &argv[argc];

		/* read the arguments */
		int arg_i = 0;
		int env_i = 0;
		node.for_each_sub_node([&] (Xml_node const &node) {

			/* insert an argument */
			if (node.has_type("arg")) {

				Xml_attribute attr = node.attribute("value");
				attr.with_raw_value([&] (char const *start, size_t length) {

					size_t const size = length + 1; /* for null termination */

					argv[arg_i] = (char *)malloc(size);

					Genode::strncpy(argv[arg_i], start, size);
				});

				++arg_i;
			}
			else

			/* insert an environment variable */
			if (node.has_type("env")) try {

				auto check_attr = [] (Xml_node node, auto key) {
					if (!node.has_attribute(key))
						Genode::warning("<env> node lacks '", key, "' attribute"); };

				check_attr(node, "key");
				check_attr(node, "value");

				Xml_attribute const key   = node.attribute("key");
				Xml_attribute const value = node.attribute("value");

				using namespace Genode;

				/*
				 * An environment variable has the form <key>=<value>, followed
				 * by a terminating zero.
				 */
				size_t const var_size = key  .value_size() + 1
				                      + value.value_size() + 1;

				envp[env_i] = (char*)malloc(var_size);

				size_t pos = 0;

				/* append characters to env variable with zero termination */
				auto append = [&] (char const *s, size_t len) {

					if (pos + len >= var_size) {
						/* this should never happen */
						warning("truncated environment variable: ", node);
						return;
					}

					/* Genode's strncpy always zero-terminates */
					Genode::strncpy(envp[env_i] + pos, s, len + 1);
					pos += len;
				};

				key.with_raw_value([&] (char const *start, size_t length) {
					append(start, length); });

				append("=", 1);

				value.with_raw_value([&] (char const *start, size_t length) {
					append(start, length); });

				++env_i;

			}
			catch (Xml_node::Nonexistent_sub_node)  { }
			catch (Xml_node::Nonexistent_attribute) { }
		});

		envp[env_i] = NULL;

		/* register command-line arguments at Genode's startup code */
		genode_argc = argc;
		genode_argv = argv;
		genode_envp = environ = envp;
	});

	exit(main(genode_argc, genode_argv, genode_envp));
}


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] () { construct_component(env); });
}
