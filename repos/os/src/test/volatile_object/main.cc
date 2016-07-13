/*
 * \brief  Test for 'Volatile_object'
 * \author Norman Feske
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/volatile_object.h>
#include <base/log.h>

using Genode::Volatile_object;
using Genode::Lazy_volatile_object;
using Genode::log;


struct Object
{
	unsigned const id;

	Object(unsigned id) : id(id)
	{
		log("construct Object ", id);
	}

	~Object()
	{
		log("destruct Object ", id);
	}

	void method()             { log("method called on Object ", id); }
	void const_method() const { log("const method called on Object ", id); }
};


struct Member_with_reference
{
	Object &reference;

	const int c = 13;

	Member_with_reference(Object &reference) : reference(reference)
	{
		log("construct Member_with_reference");
	}

	~Member_with_reference()
	{
		log("destruct Member_with_reference");
	}
};


struct Compound
{
	Volatile_object<Member_with_reference>      member;
	Lazy_volatile_object<Member_with_reference> lazy_member;

	Compound(Object &object)
	:
		member(object)
	{
		log("construct Compound");
	}

	~Compound()
	{
		log("destruct Compound");
	}
};


struct Bool
{
	bool b;

	Bool(Bool const &) = delete;

	Bool(bool const &b) : b(b) { }
};


struct Throwing
{
	Throwing(Bool const &throws)
	{
		if (throws.b) {
			log("construct Throwing -> throw exception");
			throw -1;
		} else {
			log("construct Throwing -> don't throw");
		}
	}

	virtual ~Throwing()
	{
		log("destruct Throwing");
	}
};


static void call_const_method(Compound const &compound)
{
	compound.member->reference.const_method();
}


int main(int, char **)
{
	using namespace Genode;

	log("--- test-volatile_object started ---");

	{
		Object object_1(1);
		Object object_2(2);

		log("-- create Compound object --");
		Compound compound(object_1);

		log("compound.member.constructed returns ",
		    compound.member.constructed());
		log("compound.lazy_member.constructed returns ",
		    compound.lazy_member.constructed());

		log("-- construct lazy member --");
		compound.lazy_member.construct(object_2);
		log("compound.lazy_member.constructed returns ",
		    compound.lazy_member.constructed());

		log("-- call method on member (with reference to Object 1) --");
		call_const_method(compound);

		log("-- reconstruct member with Object 2 as reference --");
		compound.member.construct(object_2);

		log("-- call method on member --");
		call_const_method(compound);

		log("-- destruct member --");
		compound.member.destruct();

		log("-- try to call method on member, catch exception --");
		try {
			call_const_method(compound); }
		catch (typename Volatile_object<Member_with_reference>::Deref_unconstructed_object) {
			log("got exception, as expected"); }

		log("-- destruct Compound and Objects 1 and 2 --");
	}

	try {
		log("-- construct Throwing object");
		Bool const b_false(false), b_true(true);

		Volatile_object<Throwing> inst(b_false);
		inst.construct(b_true);
		Genode::error("expected contructor to throw");
	} catch (int i) {
		log("-- catched exception as expected");
	}

	log("--- test-volatile_object finished ---");

	return 0;
}
