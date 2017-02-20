/*
 * \brief  GDB Monitor test
 * \author Christian Prochaska
 * \date   2011-05-24
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/thread.h>
#include <timer_session/connection.h>

/* libc includes */
#include <stdio.h>

/* a variable to be modified with GDB */
int test_var = 1;

/* a thread to test GDB thread switching support */
class Test_thread : public Genode::Thread_deprecated<2*4096>
{
	public:

		Test_thread() : Thread_deprecated("test") { }

		void step_func()
		{
			/* nothing */
		}

		void sigsegv_func()
		{
			/*
			 * make sure that the main thread is sleeping in
			 * Genode::sleep_forever() when the segfault happens
			 */
			static Timer::Connection timer;
			timer.msleep(500);

			*(int *)0 = 42;
		}

		void entry() /* set a breakpoint here to test the 'info threads' command */
		{
			step_func();

			sigsegv_func();

			Genode::sleep_forever();
		}
};

/*
 * This function returns the current value of 'test_var' + 1 and can be called from
 * GDB using the 'call' or 'print' commands
 */
int test_var_func()
{
	return test_var + 1;
}

/* this function returns a value to make itself appear in the stack trace when building with -O2 */
int func2()
{
	/* set the first breakpoint here to test the 'backtrace' command for a
	 * thread which is not in a syscall */
	puts("in func2()\n");

	/* call 'test_var_func()', so the compiler does not throw the function away */
	printf("test_var_func() returned %d\n", test_var_func());

	return 0;
}


/* this function returns a value to make itself appear in the stack trace when building with -O2 */
int func1()
{
	func2();

	return 0;
}


int main(void)
{
	Test_thread test_thread;

	func1();

	test_thread.start();

	test_thread.join();

	return 0;
}

