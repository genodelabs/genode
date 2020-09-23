/*
 * \brief  Testing thread migration
 * \author Alexander Boettcher
 * \date   2020-08-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <cpu_session/connection.h>
#include <cpu_thread/client.h>
#include <timer_session/connection.h>
#include <trace_session/connection.h>

using namespace Genode;


enum { STACK_SIZE = 0x3000 };

/************************************************
 ** Migrate thread over all available CPU test **
 ************************************************/

struct Migrate_thread : Thread
{
	Blockade  blockade { };
	Env      &env;

	Migrate_thread(Env &env)
	: Thread(env, "migrate", STACK_SIZE), env(env)
	{ }

	void entry() override
	{
		Cpu_thread_client thread_client(cap());

		/* we are ready */
		blockade.wakeup();

		while (true) {
			log("[migrate] going to sleep");

			blockade.block();

			log("[migrate] woke up - got migrated ?");
		}
	}
};

struct Migrate
{
	Env               &env;
	Timer::Connection  timer  { env };
	Migrate_thread     thread { env };
	Trace::Connection  trace  { env, 15 * 4096 /* RAM quota */,
	                                 11 * 4096 /* ARG_BUFFER RAM quota */,
	                                 0         /* parent levels */ };

	Signal_handler<Migrate> timer_handler { env.ep(), *this,
	                                        &Migrate::check_traces };

	Trace::Subject_id  trace_id { };
	Affinity::Location location { };
	unsigned           loc_same { 0 };
	unsigned           loc_pos  { 0 };

	unsigned           test_rounds { 0 };

	enum State {
		LOOKUP_TRACE_ID, CHECK_AFFINITY, MIGRATE
	} state { LOOKUP_TRACE_ID };

	Migrate(Env &env) : env(env)
	{
		Affinity::Space cpus = env.cpu().affinity_space();
		log("Detected ", cpus.width(), "x", cpus.height(), " CPU",
		    cpus.total() > 1 ? "s." : ".");

		timer.sigh(timer_handler);

		thread.start();
		thread.blockade.block();

		timer.trigger_periodic( 500 * 1000 /* us */);
	}

	void check_traces()
	{
		switch (state) {
		case LOOKUP_TRACE_ID:
		{
			auto count = trace.for_each_subject_info([&](Trace::Subject_id const &id,
			                                             Trace::Subject_info const &info)
			{
				if (info.thread_name() != "migrate")
					return;

				trace_id = id;
				location = info.affinity();
				state    = CHECK_AFFINITY;

				log("[ep] thread '", info.thread_name(), "'  started,",
				    " location=", location.xpos(), "x", location.ypos());
			});

			if (count.count == count.limit && state == LOOKUP_TRACE_ID) {
				error("trace argument buffer too small for the test");
			}
			break;
		}
		case CHECK_AFFINITY:
		{
			Trace::Subject_info const info = trace.subject_info(trace_id);

			loc_same ++;

			if ((location.xpos() == info.affinity().xpos()) &&
			    (location.ypos() == info.affinity().ypos()) &&
			    (location.width() == info.affinity().width()) &&
			    (location.height() == info.affinity().height()))
			{
				if (loc_same >= 1) {
					loc_same = 0;
					state = MIGRATE;
				}
				log ("[ep] .");
				break;
			}

			loc_same = 0;
			location = info.affinity();

			log("[ep] thread '", info.thread_name(), "' migrated,",
			    " location=", location.xpos(), "x", location.ypos());

			test_rounds ++;
			if (test_rounds == 4)
				log("--- test completed successfully ---");
			break;
		}
		case MIGRATE:
			state = CHECK_AFFINITY;

			loc_pos ++;
			Affinity::Location const loc = env.cpu().affinity_space().location_of_index(loc_pos);

			/* trigger migration */
			Cpu_thread_client client(thread.cap());
			client.affinity(loc);

			log("[ep] thread 'migrate' scheduled to migrate to location=",
			    loc.xpos(), "x", loc.ypos());

			thread.blockade.wakeup();

			break;
		}
	}
};


void Component::construct(Env &env)
{
	log("--- migrate thread test started ---");

	static Migrate migrate_test(env);
}
