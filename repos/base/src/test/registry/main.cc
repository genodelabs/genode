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
#include <trace/timestamp.h>


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

static void test_performance_for_each()
{
	using namespace Genode;

	struct Item : Interface { };

	Registry<Registered<Item> >         registry { };
	Registry<Registered<Item> > const & registry_const = registry;

	static Constructible<Registered<Item>> items[20000];
	for (unsigned i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
		items[i].construct(registry);
	}

	Trace::Timestamp time_const = 0;
	Trace::Timestamp time_non_const = 0;

	/* warm up */
	registry.for_each([&](Item const &){});

	{
		Trace::Timestamp start = Trace::timestamp();

		registry_const.for_each([&](Item const &){});

		Trace::Timestamp end = Trace::timestamp();
		time_const = end - start;
	}

	{
		Trace::Timestamp start = Trace::timestamp();

		registry.for_each([](Item const &) {});

		Trace::Timestamp end = Trace::timestamp();
		time_non_const = end - start;
	}
	Genode::log("time non_const=", time_non_const, " const=", time_const,
	            " diff=", (long)(time_non_const - time_const), " ",
	            " per Item: non_const=", time_non_const / (sizeof(items)/sizeof(items[0])),
	            " const=", time_const / (sizeof(items)/sizeof(items[0])));
}

void Component::construct(Genode::Env &env)
{
	test_performance_for_each();

	test_exception_during_for_each();

	env.parent().exit(0);
}
