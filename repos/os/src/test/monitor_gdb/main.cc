/*
 * \brief  Test for the debug monitor with GDB
 * \author Christian Prochaska
 * \date   2023-07-21
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/thread.h>
#include <cpu/cache.h>

using namespace Genode;


/* a variable to be modified with GDB */
int test_var = 1;


struct Test_thread : Thread
{
	void test_step()
	{
		/* nothing */
	}

	void test_sigsegv()
	{
		*(int *)0 = 42;
	}

	Test_thread(Genode::Env &env) : Thread(env, "thread", 8192) { }

	void entry() override
	{
		test_step();
		test_sigsegv();
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

void func2()
{
	/*
	 * Set the first breakpoint here in 'Genode::cache_coherent' to test the
	 * 'backtrace' command for a thread which is not in a syscall and executes
	 * code in a shared library.
	 */
	Genode::cache_coherent(0, 0);

	/* call 'test_var_func()', so the compiler does not throw the function away */
	log("test_var_func() returned ", test_var_func());
}


void func1()
{
	func2();
}


struct Main
{
	Main(Genode::Env &env)
	{
		func1();

		Test_thread test_thread(env);
		test_thread.start();
		test_thread.join();
	}
};


void Component::construct(Env &env)
{
	static Main main { env };
}
