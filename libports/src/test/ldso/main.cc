/*
 * \brief  ldso test program
 * \author Sebastian Sumpf
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <rom_session/connection.h>
#include <base/env.h>
using namespace Genode;

/* shared-lib includes */
#include "test-ldso.h"

namespace Libc {
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
}


/********************************************************************
 ** Helpers to test construction and destruction of global objects **
 ********************************************************************/

struct Global_1
{
	int x           { 0x05060708 };
	Global_1()      { printf("%s %x\n", __func__, --x); }
	void global_1() { printf("%s %x\n", __func__, --x); }
	~Global_1()     { printf("%s %x\n", __func__, --x); x=0; }
}
global_1;

static struct Global_2
{
	int x           { 0x01020304 };
	Global_2()      { printf("%s %x\n", __func__, --x); }
	void global_2() { printf("%s %x\n", __func__, --x); }
	~Global_2()     { printf("%s %x\n", __func__, --x); x=0; }
}
global_2;


/**************************************************************************
 ** Helpers to test construction and destruction of local static objects **
 **************************************************************************/

struct Local_1
{
	int x          { 0x50607080 };
	Local_1()      { printf("%s %x\n", __func__, --x); }
	void local_1() { printf("%s %x\n", __func__, --x); }
	~Local_1()     { printf("%s %x\n", __func__, --x); x=0; }
};

struct Local_2
{
	int x          { 0x10203040 };
	Local_2()      { printf("%s %x\n", __func__, --x); }
	void local_2() { printf("%s %x\n", __func__, --x); }
	~Local_2()     { printf("%s %x\n", __func__, --x); x=0; }
};

Local_1 * local_1()
{
	static Local_1 s;
	return &s;
}

static Local_2 * local_2()
{
	static Local_2 s;
	return &s;
}


/************************************************************************
 ** Helpers to test function attributes 'constructor' and 'destructor' **
 ************************************************************************/

       unsigned pod_1 { 0x80706050 };
static unsigned pod_2 { 0x40302010 };

static void attr_constructor_1()__attribute__((constructor));
       void attr_constructor_2()__attribute__((constructor));

       void attr_destructor_1() __attribute__((destructor));
static void attr_destructor_2() __attribute__((destructor));

static void attr_constructor_1() { printf("%s %x\n", __func__, --pod_1); }
       void attr_constructor_2() { printf("%s %x\n", __func__, --pod_2); }

       void attr_destructor_1()  { printf("%s %x\n", __func__, --pod_1); pod_1=0; }
static void attr_destructor_2()  { printf("%s %x\n", __func__, --pod_2); pod_2=0; }


/********************************************
 ** Helpers to test C++ exception handling **
 ********************************************/

static void exception() { throw 666; }

extern void __ldso_raise_exception();


/*************************************
 ** Helpers to test stack alignment **
 *************************************/

static void test_stack_align(char const *fmt, ...) __attribute__((noinline));
static void test_stack_align(char const *fmt, ...)
{
	va_list list;
	va_start(list, fmt);

	vprintf(fmt, list);

	va_end(list);
}

struct Test_stack_align_thread : Thread<0x2000>
{
	Test_stack_align_thread() : Thread<0x2000>("test_stack_align") { }
	void entry() { test_stack_align("%f\n%g\n", 3.142, 2.718); }
};

/**
 * Main function of LDSO test
 */
int main(int argc, char **argv)
{
	printf("\n");
	printf("Dynamic-linker test\n");
	printf("===================\n");
	printf("\n");

	printf("Global objects and local static objects of program\n");
	printf("--------------------------------------------------\n");
	global_1.global_1();
	global_2.global_2();
	local_1()->local_1();
	local_2()->local_2();
	printf("pod_1 %x\n", --pod_1);
	printf("pod_2 %x\n", --pod_2);
	printf("\n");

	printf("Access shared lib from program\n");
	printf("------------------------------\n");
	lib_2_global.lib_2_global();
	lib_1_local_3()->lib_1_local_3();
	printf("lib_1_pod_1 %x\n", --pod_1);

	int fd = 0;
	char buf[2];
	printf("Libc::read:\n");
	Libc::read(fd, buf, 2);

	int i = Libc::abs(-10);
	printf("Libc::abs(-10): %d\n", i);
	printf("\n");

	printf("Catch exceptions in program\n");
	printf("---------------------------\n");
	printf("exception in remote procedure call:\n");
	try { Rom_connection rom("unknown_file"); }
	catch (Rom_connection::Rom_connection_failed) { printf("caught\n"); }

	printf("exception in program: ");
	try { exception(); }
	catch (int) { printf("caught\n"); }

	printf("exception in shared lib: ");
	try { lib_1_exception(); }
	catch (Rm_session::Region_conflict) { printf("caught\n"); }

	printf("exception in dynamic linker: ");
	try { __ldso_raise_exception(); }
	catch (Genode::Exception) { printf("caught\n"); }
	printf("\n");

	lib_1_test();

	printf("test stack alignment\n");
	printf("--------------------\n");
	test_stack_align("%f\n%g\n", 3.142, 2.718);
	Test_stack_align_thread t;
	t.start();
	t.join();
	printf("\n");

	/* test if return value is propagated correctly by dynamic linker */
	return 123;
}
