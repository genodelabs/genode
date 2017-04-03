/*
 * \brief  ldso test program
 * \author Sebastian Sumpf
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/printf.h>
#include <rom_session/connection.h>
#include <base/env.h>
#include <base/heap.h>
#include <libc/component.h>
#include <base/shared_object.h>

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

struct Test_stack_align_thread : Thread_deprecated<0x2000>
{
	Test_stack_align_thread() : Thread_deprecated<0x2000>("test_stack_align") { }
	void entry() { test_stack_align("%f\n%g\n", 3.142, 2.718); }
};


/******************
 ** Dynamic cast **
 ******************/

struct Object_base
{
	virtual void func() { printf("'Object_base' called: failed\n"); }
};

struct Object : Object_base
{
	void func() { printf("'Object' called: good\n"); }
};

void test_dynamic_cast_call(Object_base *o)
{
	Object *b = dynamic_cast<Object *>(o);
	b->func();
}

static void test_dynamic_cast(Genode::Allocator &heap)
{
	Object *o = new (heap) Object;
	test_dynamic_cast_call(o);
	destroy(heap, o);
}


/***********************
 ** Shared-object API **
 ***********************/

static void test_shared_object_api(Env &env, Allocator &alloc)
{
	/*
	 * When loading the shared object, we expect the global constructor
	 * that is present in the library to print a message.
	 *
	 * 'lib_dl_so' is a local variable such that its destructor is called
	 * when leaving the scope of the function.
	 */
	Shared_object lib_dl_so(env, alloc, "test-ldso_lib_dl.lib.so",
	                        Shared_object::BIND_LAZY, Shared_object::DONT_KEEP);
}


/**
 * Main function of LDSO test
 */
void Libc::Component::construct(Libc::Env &env)
{
	static Heap heap(env.ram(), env.rm());

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
	try { Rom_connection rom(env, "unknown_file"); }
	catch (Rom_connection::Rom_connection_failed) { printf("caught\n"); }

	printf("exception in program: ");
	try { exception(); }
	catch (int) { printf("caught\n"); }

	printf("exception in shared lib: ");
	try { lib_1_exception(); }
	catch (Region_map::Region_conflict) { printf("caught\n"); }

	printf("exception in dynamic linker: ");
	try { __ldso_raise_exception(); }
	catch (Genode::Exception) { printf("caught\n"); }
	printf("\n");

	lib_1_test();

	printf("Test stack alignment\n");
	printf("--------------------\n");
	test_stack_align("%f\n%g\n", 3.142, 2.718);
	Test_stack_align_thread t;
	t.start();
	t.join();
	printf("\n");

	printf("Dynamic cast\n");
	printf("------------\n");
	test_dynamic_cast(heap);
	printf("\n");

	printf("Shared-object API\n");
	printf("-----------------\n");
	test_shared_object_api(env, heap);
	printf("\n");

	printf("Destruction\n");
	printf("-----------\n");

	Libc::exit(123);
}
