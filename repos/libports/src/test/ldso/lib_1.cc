/*
 * \brief  ldso test library
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>

/* local includes */
#include <test-ldso.h>

using namespace Genode;


/********************************************************************
 ** Helpers to test construction and destruction of global objects **
 ********************************************************************/

struct Lib_1_global_1
{
	int x                 { 0x05060708 };
	Lib_1_global_1()      { printf("%s %x\n", __func__, --x); }
	void lib_1_global_1() { printf("%s %x\n", __func__, --x); }
	~Lib_1_global_1()     { printf("%s %x\n", __func__, --x); x=0; }
}
lib_1_global_1;

static struct Lib_1_global_2
{
	int x                 { 0x01020304 };
	Lib_1_global_2()      { printf("%s %x\n", __func__, --x); }
	void lib_1_global_2() { printf("%s %x\n", __func__, --x); }
	~Lib_1_global_2()     { printf("%s %x\n", __func__, --x); x=0; }
}
lib_1_global_2;


/**************************************************************************
 ** Helpers to test construction and destruction of local static objects **
 **************************************************************************/

struct Lib_1_local_1
{
	int x                { 0x50607080 };
	Lib_1_local_1()      { printf("%s %x\n", __func__, --x); }
	void lib_1_local_1() { printf("%s %x\n", __func__, --x); }
	~Lib_1_local_1()     { printf("%s %x\n", __func__, --x); x=0; }
};

struct Lib_1_local_2
{
	int x                { 0x10203040 };
	Lib_1_local_2()      { printf("%s %x\n", __func__, --x); }
	void lib_1_local_2() { printf("%s %x\n", __func__, --x); }
	~Lib_1_local_2()     { printf("%s %x\n", __func__, --x); x=0; }
};

Lib_1_local_3::Lib_1_local_3()      { printf("%s %x\n", __func__, --x); }
void Lib_1_local_3::lib_1_local_3() { printf("%s %x\n", __func__, --x); }
Lib_1_local_3::~Lib_1_local_3()     { printf("%s %x\n", __func__, --x); x=0; }

Lib_1_local_1 * lib_1_local_1()
{
	static Lib_1_local_1 s;
	return &s;
}

static Lib_1_local_2 * lib_1_local_2()
{
	static Lib_1_local_2 s;
	return &s;
}

Lib_1_local_3 * lib_1_local_3()
{
	static Lib_1_local_3 s;
	return &s;
}


/************************************************************************
 ** Helpers to test function attributes 'constructor' and 'destructor' **
 **                                                                    **
 ** FIXME: We don't assume attribute 'destructor' to work in shared    **
 **        libraries by now as dtors of such libraries are not called  **
 **        by the dynamic linker.                                      **
 ************************************************************************/

       unsigned lib_1_pod_1 { 0x80706050 };
static unsigned lib_1_pod_2 { 0x40302010 };

static void lib_1_attr_constructor_1()__attribute__((constructor));
       void lib_1_attr_constructor_2()__attribute__((constructor));

       void lib_1_attr_destructor_1() __attribute__((destructor));
static void lib_1_attr_destructor_2() __attribute__((destructor));

static void lib_1_attr_constructor_1() { printf("%s %x\n", __func__, --lib_1_pod_1); }
       void lib_1_attr_constructor_2() { printf("%s %x\n", __func__, --lib_1_pod_2); }

       void lib_1_attr_destructor_1()  { printf("%s %x\n", __func__, --lib_1_pod_1); lib_1_pod_1=0; }
static void lib_1_attr_destructor_2()  { printf("%s %x\n", __func__, --lib_1_pod_2); lib_1_pod_2=0; }



static void exception() { throw 666; }

void lib_1_exception() { throw Genode::Region_map::Region_conflict(); }
void lib_1_good() { }


void lib_1_test()
{
	printf("global objects and local static objects of shared lib\n");
	printf("-----------------------------------------------------\n");
	lib_1_global_1.lib_1_global_1();
	lib_1_global_2.lib_1_global_2();
	lib_1_local_1()->lib_1_local_1();
	lib_1_local_2()->lib_1_local_2();
	printf("lib_1_pod_1 %x\n", --lib_1_pod_1);
	printf("lib_1_pod_2 %x\n", --lib_1_pod_2);
	printf("\n");

	printf("Access shared lib from another shared lib\n");
	printf("-----------------------------------------\n");
	lib_2_global.lib_2_global();
	lib_2_local()->lib_2_local();
	printf("lib_2_pod_1 %x\n", --lib_2_pod_1);
	printf("\n");

	printf("Catch exceptions in shared lib\n");
	printf("------------------------------\n");
	printf("exception in lib: ");
	try { exception(); }
	catch (...) { printf("caught\n"); }

	printf("exception in another shared lib: ");
	try { lib_2_exception(); }
	catch(...) { printf("caught\n"); }
	printf("\n");
}
