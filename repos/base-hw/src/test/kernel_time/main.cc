/*
 * \brief  Test accuracy (i.e. time drift) of Kernel::time()
 * \author Johannes Schlatow
 * \date   2025-09-19
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>

/* core includes */
#include <kernel/interface.h>

using namespace Genode;

struct Probe : Thread
{
	Env &env;

	Timer::Connection       timer { env };

	Kernel::time_t volatile last_time { 0 };

	uint64_t                iteration { 0 };

	Probe(Env &env, Location const &location)
	: Thread(env, Name("probe"), Stack_size { 4096 }, location),
	  env(env)
	{ }
		
	void entry() override
	{
		while (true) {
			if (iteration++ % 1000 == 0)
				last_time = Kernel::time();

			timer.msleep(10);
		}
	}
	
};


struct Main
{
	Env &env;

	Timer::Connection timer { env };

	Probe             probe { env, env.cpu().affinity_space().location_of_index(1) };

	uint64_t          iterations { 10 };

	enum { MIN_PREEMPTION_US = 100 };
	enum { MAX_DRIFT_US      = 1000 };

	Main(Env &env) : env(env)
	{
		if (env.cpu().affinity_space().total() == 1) {
			error("Test requires SMP");
			env.parent().exit(1);
		}

		timer.msleep(5000);
		probe.start();

		for (; iterations; --iterations) {

			Kernel::time_t last_probe_time = probe.last_time;
			Kernel::time_t probe_time      = last_probe_time;
			Kernel::time_t ref_time        = Kernel::time();
			Kernel::time_t last_ref_time;
			while (last_probe_time == probe_time) {
				last_ref_time = ref_time;
				probe_time = probe.last_time;
				ref_time = Kernel::time();
			}

			bool const preempted = (ref_time - last_ref_time) > MIN_PREEMPTION_US;
			if (preempted) {
				log("preempted");
			} else {
				Kernel::time_t diff = ref_time > probe_time ? ref_time - probe_time : probe_time - ref_time;
				if (diff <= MAX_DRIFT_US)
					log("Kernel::time() drift is below threshold, current value: ", diff, "us");
				else {
					error("Kernel::time() drift reached ", diff, "us");
					env.parent().exit(1);
				}
			}
			
			timer.msleep(10*1000);
		}

		env.parent().exit(0);
	}
};


void Component::construct(Env &env) { static Main main(env); }
