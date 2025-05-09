/*
 * \brief  Test dynamic memory management in C++
 * \author Christian Helmuth
 * \date   2014-01-20
 *
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>

using Genode::log;
using Genode::destroy;


#define L() log("  ", __func__)


struct A     { int a = 0; A() { L(); } virtual ~A() { L(); } };
struct B     { int b = 0; B() { L(); } virtual ~B() { L(); } };
struct C : A { int c = 0; C() { L(); } virtual ~C() { L(); } };
struct D : B { int d = 0; D() { L(); } virtual ~D() { L(); } };


struct E : C, D
{
	int e = 0;

	E(bool thro) { L(); if (thro) { log("throw exception"); throw 1; } }

	virtual ~E() { L(); }
};


struct Allocator : Genode::Allocator
{
	Genode::Heap       _heap;
	Genode::Allocator &_a { _heap };

	Allocator(Genode::Env &env) : _heap(env.ram(), env.rm()) { }
	virtual ~Allocator() { }

	Genode::size_t consumed() const override {
		return _a.consumed(); }

	Genode::size_t overhead(Genode::size_t size) const override {
		return _a.overhead(size); }

	bool need_size_for_free() const override {
		return _a.need_size_for_free(); }

	Alloc_result try_alloc(Genode::size_t size) override
	{
		log("Allocator::alloc()");
		return _a.try_alloc(size);
	}

	void _free(Allocation &a) override { free(a.ptr, a.num_bytes); }

	void free(void *p, Genode::size_t size) override
	{
		log("Allocator::free()");
		_a.free(p, size);
	}
};


void Component::construct(Genode::Env &env)
{
	Allocator a(env);

	/***********************
	 ** Allocator pointer **
	 ***********************/

	/* test successful allocation / successful construction */
	{
		E *e = new (a)  E(false);
		destroy(a, e);
	}

	/* test successful allocation / exception in construction */
	try {
		E *e = new (a)  E(true);
		destroy(a, e);
	} catch (...) { log("exception caught"); }

	/*************************
	 ** Allocator reference **
	 *************************/

	/* test successful allocation / successful construction */
	{
		E *e = new (a)  E(false);
		destroy(a, e);
	}

	/* test successful allocation / exception in construction */
	try {
		E *e = new (a)  E(true);
		destroy(a, e);
	} catch (...) { log("exception caught"); }

	log("Test done");
}
