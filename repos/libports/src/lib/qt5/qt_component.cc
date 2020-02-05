/*
 * \brief  Entry point for Qt applications with a main() function
 * \author Christian Prochaska
 * \date   2017-05-22
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>

/* libc includes */
#include <stdlib.h> /* 'exit'   */

/* provided by the application */
extern "C" int main(int argc, char const **argv);

void initialize_qt_gui(Genode::Env &env) __attribute__((weak));
void initialize_qt_gui(Genode::Env &) { }

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		initialize_qt_gui(env);

		int argc = 1;
		char const *argv[] = { "qt5_app", 0 };

		exit(main(argc, argv));
	});
}
