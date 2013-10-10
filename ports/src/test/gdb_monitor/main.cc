/*
 * \brief  GDB Monitor thread selection and backtrace test
 * \author Christian Prochaska
 * \date   2011-05-24
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/thread.h>
#include <timer_session/connection.h>

/* libc includes */
#include <stdio.h>

class Test_thread : public Genode::Thread<2*4096>
{
	public:

		Test_thread() : Thread("test") { }

		void func()
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
			func();

			Genode::sleep_forever();
		}
};

/* this function returns a value to make itself appear in the stack trace when building with -O2 */
int func2()
{
	/* set the first breakpoint here to test the 'backtrace' command for a
	 * thread which is not in a syscall */
	puts("in func2()\n");

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

	Genode::sleep_forever();

	return 0;
}
