/*
 * \brief  Entry point for Arora
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
#include <pthread.h>

/* Qt includes */
#include <qpluginwidget/qpluginwidget.h>

/* provided by the application */
extern "C" int main(int argc, char const **argv);

extern void initialize_qt_gui(Genode::Env &);

/*
 * The main function is called from a dedicated thread, because it sometimes
 * blocks on a pthread condition variable, which prevents Genode signal
 * processing with the current implementation.
 */
void *arora_main(void *)
{
	int argc = 1;
	char const *argv[] = { "arora", 0 };

	exit(main(argc, argv));
}

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		initialize_qt_gui(env);
		QPluginWidget::env(env);

		pthread_t main_thread;
		pthread_create(&main_thread, nullptr, arora_main, nullptr);
	});
}
