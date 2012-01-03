/*
 * \brief  GDB Monitor thread selection and backtrace test
 * \author Christian Prochaska
 * \date   2011-05-24
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <timer_session/connection.h>

/* libc includes */
#include <stdio.h>

class Test_thread : public Genode::Thread<2*4096>
{
	public:

		void func3()
		{
			static Timer::Connection timer;

			while (1) {
				enum { ROUNDS = 2 };

				for (int cnt = 0; cnt < ROUNDS; ++cnt) {
					/* call libc printf function */
					printf("Test thread is running, round %d of %d\n", cnt + 1, ROUNDS);
					timer.msleep(1000);
				}

				*(int *)0 = 42;
			}

			Genode::sleep_forever();
		}

		void entry()
		{
			func3();

			Genode::sleep_forever();
		}
};


static void func2()
{
	static Timer::Connection timer;
	while(1) {
		PDBG("GDB monitor test is running...");
		timer.msleep(1000);
	}

	Genode::sleep_forever();
}

static void func1()
{
	func2();
}

int main(void)
{
	Test_thread test_thread;
	test_thread.start();

	func1();

	return 0;
}
