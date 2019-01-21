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

#include <base/log.h>
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
	Global_1()      { log(__func__, " ", Hex(--x)); }
	void global_1() { log(__func__, " ", Hex(--x)); }
	~Global_1()     { log(__func__, " ", Hex(--x)); x=0; }
}
global_1;

static struct Global_2
{
	int x           { 0x01020304 };
	Global_2()      { log(__func__, " ", Hex(--x)); }
	void global_2() { log(__func__, " ", Hex(--x)); }
	~Global_2()     { log(__func__, " ", Hex(--x)); x=0; }
}
global_2;


/**************************************************************************
 ** Helpers to test construction and destruction of local static objects **
 **************************************************************************/

struct Local_1
{
	int x          { 0x50607080 };
	Local_1()      { log(__func__, " ", Hex(--x)); }
	void local_1() { log(__func__, " ", Hex(--x)); }
	~Local_1()     { log(__func__, " ", Hex(--x)); x=0; }
};

struct Local_2
{
	int x          { 0x10203040 };
	Local_2()      { log(__func__, " ", Hex(--x)); }
	void local_2() { log(__func__, " ", Hex(--x)); }
	~Local_2()     { log(__func__, " ", Hex(--x)); x=0; }
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

static void attr_constructor_1() { log(__func__, " ", Hex(--pod_1)); }
       void attr_constructor_2() { log(__func__, " ", Hex(--pod_2)); }

       void attr_destructor_1()  { log(__func__, " ", Hex(--pod_1)); pod_1=0; }
static void attr_destructor_2()  { log(__func__, " ", Hex(--pod_2)); pod_2=0; }


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

	char buf[128] { };
	String_console(buf, sizeof(buf)).vprintf(fmt, list);
	log(Cstring(buf));

	va_end(list);
}

struct Test_stack_align_thread : Thread
{
	Test_stack_align_thread(Env &env)
	: Thread(env, "test_stack_align", 0x2000) { }

	void entry() { test_stack_align("%f\n%g\n", 3.142, 2.718); }
};


/******************
 ** Dynamic cast **
 ******************/

struct Object_base
{
	virtual void func() { log("'Object_base' called: failed"); }
};

struct Object : Object_base
{
	void func() { log("'Object' called: good"); }
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

	using Genode::log;

	log("");
	log("Dynamic-linker test");
	log("===================");
	log("");

	log("Global objects and local static objects of program");
	log("--------------------------------------------------");
	global_1.global_1();
	global_2.global_2();
	local_1()->local_1();
	local_2()->local_2();
	log("pod_1 ", Hex(--pod_1));
	log("pod_2 ", Hex(--pod_2));
	log("");

	log("Access shared lib from program");
	log("------------------------------");
	lib_2_global.lib_2_global();
	lib_1_local_3()->lib_1_local_3();
	log("lib_1_pod_1 ", Hex(--pod_1));

	int fd = STDERR_FILENO + 1;
	char buf[2] { };
	log("Libc::read:");
	Libc::read(fd, buf, 2);

	int i = Libc::abs(-10);
	log("Libc::abs(-10): ", i);
	log("");

	log("Catch exceptions in program");
	log("---------------------------");
	try {
		Rom_connection rom(env, "unknown_rom");
		error("undelivered exception in remote procedure call");
	}
	catch (Rom_connection::Rom_connection_failed) {
		log("exception in remote procedure call: caught");
	}

	try {
		exception();
		error("undelivered exception in program");
	}
	catch (int) { log("exception in program: caught"); }

	try {
		lib_1_exception();
		error("undelivered exception in shared lib");
	}
	catch (Region_map::Region_conflict) { log("exception in shared lib: caught"); }

	try {
		__ldso_raise_exception();
		log("undelivered exception in dynamic linker");
	}
	catch (Genode::Exception) { log("exception in dynamic linker: caught"); }
	log("");

	lib_1_test();

	log("Test stack alignment");
	log("--------------------");
	test_stack_align("%f\n%g\n", 3.142, 2.718);
	Test_stack_align_thread t { env };
	t.start();
	t.join();

	log("Dynamic cast");
	log("------------");
	test_dynamic_cast(heap);
	log("");

	log("Shared-object API");
	log("-----------------");
	test_shared_object_api(env, heap);
	log("");

	log("Destruction");
	log("-----------");

	Libc::exit(123);
}
