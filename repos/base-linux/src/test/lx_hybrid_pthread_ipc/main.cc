/*
 * \brief  Test for performing IPC from a pthread created outside of Genode
 * \author Norman Feske
 * \date   2011-12-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/thread.h>
#include <base/log.h>

/* libc includes */
#include <pthread.h>
#include <stdlib.h>



static Genode::Lock *main_wait_lock()
{
	static Genode::Lock inst(Genode::Lock::LOCKED);
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

	main_wait_lock()->unlock();
	return 0;
}


static int exit_status;
static void exit_on_suspended() { exit(exit_status); }


Genode::size_t Component::stack_size() { return 16*1024*sizeof(long); }


/*
 * Component implements classical main function in construct.
 */
void Component::construct(Genode::Env &env)
{
	Genode::log("--- pthread IPC test ---");

	/* create thread w/o Genode's thread API */
	pthread_t pth;
	pthread_create(&pth, 0, pthread_entry, 0);

	/* wait until 'pthread_entry' finished */
	main_wait_lock()->lock();

	Genode::log("--- finished pthread IPC test ---");
	exit_status = 0;
	env.ep().schedule_suspend(exit_on_suspended, nullptr);
}
