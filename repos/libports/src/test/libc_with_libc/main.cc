/*
 * \brief  Test Libc::with_libc nesting
 * \author Josef Soentgen
 * \date   2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <libc/component.h>

/* libc includes */
#include <sys/select.h>
#include <stdio.h>


using namespace Genode;


struct Test
{
	int _id;

	Test(Env &env, int id) : _id(id) { log("Start test ", _id); }

	virtual ~Test() { log("Finished test ", _id); }
};


/*
 * Explicitly_nested test
 *
 * Call with_libc from within with_libc.
 */
struct Explicitly_nested : Test
{
	Explicitly_nested(Env &env, int id) : Test(env, id)
	{
		log("calling with_libc");
		Libc::with_libc([&] () {

			log("calling with_libc nested");
			Libc::with_libc([&] () {
				printf("Hello from with_libc nested\n");
			});
		});
	}
};


/*
 * App_signal_deferred test
 *
 * Application-level signals do not interrupt blocking libc calls but are
 * deferred until the component returns to the entrypoint event loop.
 */
struct App_signal_deferred : Test
{
	Env &_env;

	void _handle()
	{
		error("application-level signal was dispatched during select()");
	}

	Signal_handler<App_signal_deferred> _dispatcher {
		_env.ep(), *this, &App_signal_deferred::_handle };

	App_signal_deferred(Env &env, int id)
	: Test(env, id), _env(env)
	{
		log("calling with_libc");

		Libc::with_libc([&] () {
			Signal_transmitter(_dispatcher).submit();

			unsigned const secs = 3;

			log("calling select with ", secs, " secs timeout from with_libc");

			timeval to { secs, 0 };
			select(0, nullptr, nullptr, nullptr, &to);
		});
	}
};


/*
 * Explicitly_triple_nested test
 *
 * Call with_libc from within with_libc from within with_libc.
 */
struct Explicitly_triple_nested : Test
{
	Explicitly_triple_nested(Env &env, int id) : Test(env, id)
	{
		log("calling with_libc");
		Libc::with_libc([&] () {

			log("calling with_libc nested");
			Libc::with_libc([&] () {

				log("calling with_libc nested again");
				Libc::with_libc([&] () {

					printf("Hello from with_libc nested again\n");
				});
			});
		});
	}
};


struct Main
{
	Constructible<Explicitly_nested>        test_1;
	Constructible<App_signal_deferred>      test_2;
	Constructible<Explicitly_triple_nested> test_3;

	Main(Env &env)
	{
		log("--- start with_libc tests ---");

		test_1.construct(env, 1); test_1.destruct();
		test_2.construct(env, 2); test_2.destruct();
		test_3.construct(env, 3); test_3.destruct();

		log("--- finished with_libc tests ---");
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main main(env); }
