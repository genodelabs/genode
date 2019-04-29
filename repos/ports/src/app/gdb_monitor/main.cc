/*
 * \brief  GDB Monitor
 * \author Christian Prochaska
 * \date   2010-09-16
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <libc/component.h>

/*
 * Suppress messages of libc dummy functions
 */
extern "C" int _sigaction()   { return -1; }
extern "C" int  getpid()      { return -1; }
extern "C" int  sigprocmask() { return -1; }
extern "C" int _sigprocmask() { return -1; }
extern "C" int  sigsuspend()  { return -1; }

/*
 * version.c
 */
extern "C" const char version[] = "8.1.1";
extern "C" const char host_name[] = "";


extern int gdbserver_main(int argc, char *argv[]);

extern Genode::Env *genode_env;

void Libc::Component::construct(Libc::Env &env)
{
	genode_env = &env;

	int argc = 3;
	char *argv[] = { "gdbserver", "/dev/terminal", "target", 0 };

	Libc::with_libc([&] () {
		gdbserver_main(argc, argv);
	});
}
