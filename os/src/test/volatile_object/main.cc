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
#include <base/printf.h>

using Genode::Volatile_object;
using Genode::Lazy_volatile_object;


struct Object
{
	unsigned const id;

	Object(unsigned id) : id(id)
	{
		PLOG("construct Object %d", id);
	}

	~Object()
	{
		PLOG("destruct Object %d", id);
	}

	void method()             { PLOG("method called on Object %d", id); }
	void const_method() const { PLOG("const method called on Object %d", id); }
};


struct Member_with_reference
{
	Object &reference;

	const int c = 13;

	Member_with_reference(Object &reference) : reference(reference)
	{
		PLOG("construct Member_with_reference");
	}

	~Member_with_reference()
	{
		PLOG("destruct Member_with_reference");
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
		PLOG("construct Compound");
	}

	~Compound()
	{
		PLOG("destruct Compound");
	}
};


static void call_const_method(Compound const &compound)
{
	compound.member->reference.const_method();
}


int main(int, char **)
{
	using namespace Genode;

	printf("--- test-volatile_object started ---\n");

	{
		Object object_1(1);
		Object object_2(2);

		printf("-- create Compound object --\n");
		Compound compound(object_1);

		PLOG("compound.member.is_constructed returns %d",
		     compound.member.is_constructed());
		PLOG("compound.lazy_member.is_constructed returns %d",
		     compound.lazy_member.is_constructed());

		printf("-- construct lazy member --\n");
		compound.lazy_member.construct(object_2);
		PLOG("compound.lazy_member.is_constructed returns %d",
		     compound.lazy_member.is_constructed());

		printf("-- call method on member (with reference to Object 1) --\n");
		call_const_method(compound);

		printf("-- reconstruct member with Object 2 as reference --\n");
		compound.member.construct(object_2);

		printf("-- call method on member --\n");
		call_const_method(compound);

		printf("-- destruct member --\n");
		compound.member.destruct();

		printf("-- try to call method on member, catch exception --\n");
		try {
			call_const_method(compound); }
		catch (typename Volatile_object<Member_with_reference>::Deref_unconstructed_object) {
			PLOG("got exception, as expected"); }

		printf("-- destruct Compound and Objects 1 and 2 --\n");
	}

	printf("--- test-volatile_object finished ---\n");

	return 0;
}
