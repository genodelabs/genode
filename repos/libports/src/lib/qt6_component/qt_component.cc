/*
 * \brief  Entry point for Qt applications with a main() function
 * \author Christian Prochaska
 * \date   2017-05-22
 */

/*
 * Copyright (C) 2017-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/args.h>
#include <libc/component.h>

/* libc includes */
#include <stdlib.h> /* 'exit'    */

/* qt6_component includes */
#include <qt6_component/qpa_init.h>

/* initial environment for the FreeBSD libc implementation */
extern char **environ;

/* provided by the application */
extern "C" int main(int argc, char **argv, char **envp);

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		qpa_init(env);

		int argc    = 0;
		char **argv = nullptr;
		char **envp = nullptr;

		populate_args_and_env(env, argc, argv, envp);

		/* at least the executable name is required */

		char default_argv0[] { "qt6_component" };
		char *default_argv[] { default_argv0, nullptr };

		if (argc == 0) {
			argc = 1;
			argv = default_argv;
		}

		environ = envp;

		exit(main(argc, argv, envp));
	});
}
