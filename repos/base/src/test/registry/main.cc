/*
 * \brief  Test for 'Registry' data structure
 * \author Norman Feske
 * \date   2018-08-21
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/registry.h>
#include <base/component.h>
#include <base/log.h>


/*
 * Check that an exception that occurs during the iteration over
 * registry items does not affect the registry content.
 */
static void test_exception_during_for_each()
{
	using namespace Genode;

	struct Item : Interface
	{
		typedef String<10> Name;
		Name const name;

		Item(Name const &name) : name(name) { }

		class Invalid : Exception { };

		void visit()
		{
			if (name == "second")
				throw Invalid();
		}
	};

	Registry<Registered<Item> > items { };

	Registered<Item> first (items, "first");
	Registered<Item> second(items, "second");
	Registered<Item> third (items, "third");

	auto num_items = [&] () {
		unsigned cnt = 0;
		items.for_each([&] (Item &) { cnt++; });
		return cnt;
	};

	unsigned const num_items_before_exception = num_items();

	try {
		items.for_each([&] (Item &item) {
			/* second item throws an exception */
			item.visit();
		});
	}
	catch (Item::Invalid) { log("exception occurred (expected)"); };

	unsigned const num_items_after_exception = num_items();

	struct Failed : Exception { };
	if (num_items_before_exception != num_items_after_exception)
		throw Failed();
}


void Component::construct(Genode::Env &env)
{
	test_exception_during_for_each();

	env.parent().exit(0);
}
