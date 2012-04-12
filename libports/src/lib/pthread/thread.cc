/*
 * \brief  POSIX thread implementation
 * \author Christian Prochaska
 * \date   2012-03-12
 *
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>

#include <errno.h>
#include <pthread.h>

using namespace Genode;

extern "C" {

	enum { STACK_SIZE=64*1024 };

	/*
	 * This class is named 'struct pthread' because the 'pthread_t' type is
	 * defined as 'struct pthread*' in '_pthreadtypes.h'
	 */
	struct pthread : Thread<STACK_SIZE>
	{
		void *(*_start_routine) (void *);
		void *_arg;

		pthread(void *(*start_routine) (void *), void *arg)
		: Thread<STACK_SIZE>("pthread"),
		  _start_routine(start_routine),
		  _arg(arg) { }

		void entry()
		{
			void *exit_status = _start_routine(_arg);
			pthread_exit(exit_status);
		}
	};


	int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	                   void *(*start_routine) (void *), void *arg)
	{
		pthread_t thread_obj = new (env()->heap()) pthread(start_routine, arg);

		if (!thread_obj) {
			errno = EAGAIN;
			return -1;
		}

		*thread = thread_obj;

		thread_obj->start();

		return 0;
	}


	int pthread_cancel(pthread_t thread)
	{
		destroy(env()->heap(), thread);
		return 0;
	}


	void pthread_exit(void *value_ptr)
	{
		pthread_cancel(pthread_self());
		sleep_forever();
	}


	pthread_t pthread_self(void)
	{
		static struct pthread main_thread(0, 0);

		pthread_t thread = static_cast<pthread_t>(Thread_base::myself());

		/* the main thread does not have a Genode thread object */
		return thread ? thread : &main_thread;
	}

}
