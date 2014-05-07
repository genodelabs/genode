/*
 * \brief  Test dynamic memory management in C++
 * \author Christian Helmuth
 * \date   2014-01-20
 *
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/env.h>

using Genode::printf;
using Genode::destroy;


#define L() printf("  %s\n", __func__)


struct A     { int a; A() { L(); } virtual ~A() { L(); } };
struct B     { int b; B() { L(); } virtual ~B() { L(); } };
struct C : A { int c; C() { L(); } virtual ~C() { L(); } };
struct D : B { int d; D() { L(); } virtual ~D() { L(); } };


struct E : C, D
{
	int e;

	E(bool thro) { L(); if (thro) { printf("throw exception\n"); throw 1; } }

	virtual ~E() { L(); }
};


struct Allocator : Genode::Allocator
{
	Genode::Allocator &heap = *Genode::env()->heap();

	Allocator() { }
	virtual ~Allocator() { }

	Genode::size_t consumed() override
	{
		return heap.consumed();
	}

	Genode::size_t overhead(Genode::size_t size) override
	{
		return heap.overhead(size);
	}

	bool need_size_for_free() const override
	{
		return heap.need_size_for_free();
	}

	bool alloc(Genode::size_t size, void **p) override
	{
		*p = heap.alloc(size);

		printf("Allocator::alloc()\n");

		return *p != 0;
	}

	void free(void *p, Genode::size_t size) override
	{
		printf("Allocator::free()\n");
		heap.free(p, size);
	}
};


int main()
{
	Allocator a;

	/***********************
	 ** Allocator pointer **
	 ***********************/

	/* test successful allocation / successful construction */
	{
		E *e = new (&a)  E(false);
		destroy(&a, e);
	}

	/* test successful allocation / exception in construction */
	try {
		E *e = new (&a)  E(true);
		destroy(&a, e);
	} catch (...) { printf("exception caught\n"); }

	/*************************
	 ** Allocator reference **
	 *************************/

	/* test successful allocation / successful construction */
	{
		E *e = new (a)  E(false);
		destroy(&a, e);
	}

	/* test successful allocation / exception in construction */
	try {
		E *e = new (a)  E(true);
		destroy(&a, e);
	} catch (...) { printf("exception caught\n"); }
}
