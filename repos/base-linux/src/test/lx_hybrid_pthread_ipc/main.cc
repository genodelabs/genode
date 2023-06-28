/*
 * \brief  Test for performing IPC from a pthread created outside of Genode
 * \author Norman Feske
 * \date   2011-12-20
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/thread.h>
#include <base/log.h>

/* libc includes */
#pragma GCC diagnostic ignored "-Weffc++"
#include <pthread.h>
#pragma GCC diagnostic pop
#include <stdlib.h>


static Genode::Blockade *main_wait_lock()
{
	static Genode::Blockade inst;
	return &inst;
}


static void *pthread_entry(void *)
{
	Genode::log("first message");

	/*
	 * Without the lazy initialization of 'Thread' objects for threads
	 * created w/o Genode's Thread API, the printing of the first message will
	 * never return because the IPC reply could not be delivered.
	 *
	 * With the on-demand creation of 'Thread' objects, the second message
	 * will appear in the LOG output.
	 */

	Genode::log("second message");

	main_wait_lock()->wakeup();
	return 0;
}


void Component::construct(Genode::Env &env)
{
	Genode::log("--- pthread IPC test ---");

	/* create thread w/o Genode's thread API */
	pthread_t pth;
	pthread_create(&pth, 0, pthread_entry, 0);

	/* wait until 'pthread_entry' finished */
	main_wait_lock()->block();

	Genode::log("--- finished pthread IPC test ---");
	env.parent().exit(0);
}
