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
#include <stdlib.h> /* for 'exit' */

extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;


/* provided by the application */
extern "C" int main(int argc, char ** argv, char **envp);


void Libc::Component::construct(Genode::Env &env)
{
	exit(main(genode_argc, genode_argv, genode_envp));
}
