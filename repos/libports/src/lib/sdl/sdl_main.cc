/*
 * \brief  Entry point for SDL applications with a main() function
 * \author Josef Soentgen
 * \date   2017-11-21
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <libc/component.h>

/* libc includes */
#include <stdlib.h> /* 'malloc' and 'exit' */
#include <pthread.h>

extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;

/* initial environment for the FreeBSD libc implementation */
extern char **environ;

/* provided by the application */
extern "C" int main(int argc, char *argv[], char *envp[]);


/* provided by our SDL backend */
extern void sdl_init_genode(Genode::Env &env);



static void* sdl_main(void *)
{
	exit(main(genode_argc, genode_argv, genode_envp));
	return nullptr;
}


void Libc::Component::construct(Libc::Env &env)
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
			if (node.has_type("arg")) try {
				Xml_attribute attr = node.attribute("value");

				Genode::size_t const arg_len = attr.value_size()+1;
				char *arg = argv[arg_i] = (char*)malloc(arg_len);

				attr.value(arg, arg_len);
				++arg_i;

			} catch (Xml_node::Nonexistent_sub_node) { }

			else

			/* insert an environment variable */
			if (node.has_type("env")) try {
				Xml_attribute key_attr = node.attribute("key");
				Xml_attribute val_attr = node.attribute("value");

				Genode::size_t const pair_len =
					key_attr.value_size() +
					val_attr.value_size() + 1;
				char *env = envp[env_i] = (char*)malloc(pair_len);

				Genode::size_t off = 0;
				key_attr.value(&env[off], key_attr.value_size()+1);
				off = key_attr.value_size();
				env[off++] = '=';
				val_attr.value(&env[off], val_attr.value_size()+1);
				++env_i;

			} catch (Xml_node::Nonexistent_sub_node) { }
		});

		envp[env_i] = NULL;

		/* register command-line arguments at Genode's startup code */
		genode_argc = argc;
		genode_argv = argv;
		genode_envp = environ = envp;
	});

	/* pass env to SDL backend */
	sdl_init_genode(env);

	pthread_attr_t attr;
	pthread_t      main_thread;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 768 * 1024);

	if (pthread_create(&main_thread, &attr, sdl_main, nullptr)) {
		Genode::error("failed to create SDL main thread");
		exit(1);
	}
}
