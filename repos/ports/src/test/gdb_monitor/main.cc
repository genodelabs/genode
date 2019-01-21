/*
 * \brief  GDB Monitor test
 * \author Christian Prochaska
 * \date   2011-05-24
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pthread.h>
#include <stdio.h>


/* a variable to be modified with GDB */
int test_var = 1;


void test_thread_step()
{
	/* nothing */
}


void test_thread_sigsegv()
{
	*(int *)0 = 42;
}


void *test_thread_start(void*)
{
	test_thread_step();
	test_thread_sigsegv();
	return nullptr;
}


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


int main()
{
	func1();

	pthread_t test_thread;
	pthread_create(&test_thread, nullptr, test_thread_start, nullptr);
	pthread_join(test_thread, nullptr);

	return 0;
}
