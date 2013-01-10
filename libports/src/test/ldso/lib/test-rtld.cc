/*
 * \brief  ldso test library
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
/* shared-lib includes */
#include "test-ldso.h"

using namespace Genode;

class Static_test 
{
	public:
		void print_signature()
		{
			printf("a: %08lx b: %08lx c: %08lx 6: %08lx\n", _a, _b, _c, _6);
		}

		Static_test() : _a(0xaaaaaaaa), _b(0xbbbbbbbb),
		                _c(0xcccccccc), _6(0x666)
		{}

	private:
		unsigned long _a, _b, _c, _6;

};

static Static_test _static;

static void __raise_exception()
{
	throw 666;
}

void Link::raise_exception()
{
	throw Genode::Rm_session::Region_conflict();
}

void Link::static_function_object()
{
	static Static_test local_static;
	local_static.print_signature();
}

void Link::dynamic_link_test()
{
	printf("good\n");

	printf("Ctor in shared lib ... ");
	_static.print_signature();

	printf("Exception in shared lib ... ");

	try {
		__raise_exception();
	}
	catch (...) {
		printf("good (library)\n");
	}

	printf("Cross library exception ... ");

	try {
		Link::cross_lib_exception();
	}
	catch(...) {
		printf("good (cross library)\n");
	}
}
