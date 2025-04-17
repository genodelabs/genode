/*
 * \brief  Unit-test scheduler implementation of the kernel
 * \author Stefan Kalkowski
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>

/* core includes */
#include <kernel/scheduler.h>

using namespace Genode;

namespace Scheduler_test { struct Main; }


struct Scheduler_test::Main
{
	using time_t = Kernel::time_t;
	using Scheduler = Kernel::Scheduler;

	struct Context : Scheduler::Context
	{
		using Label = String<32>;

		Label const _label;

		Context(Kernel::Scheduler::Group_id const id,
		        Label const &label)
		:
			Kernel::Scheduler::Context { id },
			_label { label }
		{ }

		Label const &label() const { return _label; }
	};

	Env &env;

	enum Id {
		IDLE,
		DRV1, DRV2, DRV3,
		MUL1, MUL2, MUL3,
		APP1, APP2, APP3,
		BCK1, BCK2, BCK3,
		MAX = BCK3
	};

	using Gids = Scheduler::Group_id::Ids;

	Context contexts[MAX+1] {
		[IDLE] = { Gids::INVALID,    "idle"        },
		[DRV1] = { Gids::DRIVER,     "driver1"     },
		[DRV2] = { Gids::DRIVER,     "driver2"     },
		[DRV3] = { Gids::DRIVER,     "driver3"     },
		[MUL1] = { Gids::MULTIMEDIA, "multimedia1" },
		[MUL2] = { Gids::MULTIMEDIA, "multimedia2" },
		[MUL3] = { Gids::MULTIMEDIA, "multimedia3" },
		[APP1] = { Gids::APP,        "app1"        },
		[APP2] = { Gids::APP,        "app2"        },
		[APP3] = { Gids::APP,        "app3"        },
		[BCK1] = { Gids::BACKGROUND, "background1" },
		[BCK2] = { Gids::BACKGROUND, "background2" },
		[BCK3] = { Gids::BACKGROUND, "background3" }
	};

	Kernel::Timer timer {};
	Scheduler scheduler { timer, contexts[IDLE] };

	Context& current() {
		return static_cast<Context&>(scheduler.current()); }

	Context::Label const &label(Kernel::Scheduler::Context &c) const {
		return static_cast<Context&>(c).label(); }

	void dump()
	{
		log("");
		log("Scheduler state: (time=", timer.time(),
		    " min_vtime=", scheduler._min_vtime, " ",
		    " timeout=", timer._next_timeout, ")");
		unsigned i = 0;
		scheduler._for_each_group([&] (Scheduler::Group &group) {
			log("Group ", i++, " (weight=", group._weight,
			    ", warp=", group._warp,
			    ") has vtime: ", group._vtime,
			    " and min_vtime: ", group._min_vtime);

			using List_element = Genode::List_element<Scheduler::Context>;

			if (group._contexts.first()) log("  Contexts:");
			for (List_element * le = group._contexts.first(); le;
			     le = le->next()) {
			Scheduler::Context &c = *le->object();
				log("    ", label(c), " has vtime: ", c.vtime(),
				    " and real execution time: ", c.execution_time());
			}
		});

		log("Current context: ", current().label(),
		    " (group=", current()._id.value,
		    ") has vtime: ", current().vtime(),
		    " and real execution time: ", current().execution_time());
	}

	void update_and_check(time_t   const consumed_abs_time,
	                      Id       const expected_current,
	                      time_t   const expected_abs_timeout,
	                      unsigned const line_nr)
	{
		timer.set_time(consumed_abs_time);
		scheduler.update();

		if (&current() != &contexts[expected_current]) {
			error("wrong current context ", current().label(),
			      " in line ", line_nr);
			dump();
			env.parent().exit(-1);
		}

		if (timer._next_timeout != expected_abs_timeout) {
			error("expected timeout ", expected_abs_timeout,
			      " in line ", line_nr);
			error("But actual timeout is: ", timer._next_timeout);
			dump();
			env.parent().exit(-1);
		}
	}

	void test_background_idle();
	void test_one_per_group();
	void test_io_signal();
	void test_all_and_yield();

	Main(Env &env) : env(env)
	{
		/*
		 * Set fixed values for min timeout and group weights and warp
		 * values here, because they may change within the kernel, but
		 * the algorithm logic gets tested here instead
		 */
		scheduler._min_timeout = 500;
		construct_at<Scheduler::Group>(&scheduler._groups[Gids::DRIVER], 2, 400);
		construct_at<Scheduler::Group>(&scheduler._groups[Gids::MULTIMEDIA], 3, 200);
		construct_at<Scheduler::Group>(&scheduler._groups[Gids::APP], 2, 100);
		construct_at<Scheduler::Group>(&scheduler._groups[Gids::BACKGROUND], 1, 0);
	}
};


void Scheduler_test::Main::test_background_idle()
{
	time_t MAX_TIME = scheduler._max_timeout;

	/* params:       time, curr, timeout, line */
	update_and_check(   0, IDLE,       0, __LINE__);
	scheduler.ready(contexts[BCK1]);
	update_and_check(   0, BCK1, MAX_TIME, __LINE__);
	update_and_check(  10, BCK1, MAX_TIME, __LINE__);
	update_and_check(   0, BCK1, MAX_TIME, __LINE__);
	scheduler.ready(contexts[BCK2]);
	update_and_check(  10, BCK2,      510, __LINE__);
	update_and_check( 510, BCK1,     1510, __LINE__);
	update_and_check(1530, BCK2,     2550, __LINE__);
	scheduler.ready(contexts[BCK3]);
	update_and_check(2000, BCK3,     2500, __LINE__);
	update_and_check(2500, BCK2,     3050, __LINE__);
	update_and_check(3050, BCK1,     4000, __LINE__);
	scheduler.unready(contexts[BCK1]);
	update_and_check(3100, BCK3,     3650, __LINE__);
	scheduler.unready(contexts[BCK3]);
	update_and_check(3040, BCK2, MAX_TIME
	                               + 3040, __LINE__);
	update_and_check(4000, BCK2, MAX_TIME
	                               + 4000, __LINE__);
}


void Scheduler_test::Main::test_one_per_group()
{
	scheduler.ready(contexts[BCK1]);
	scheduler.ready(contexts[APP1]);
	scheduler.ready(contexts[DRV1]);
	scheduler.ready(contexts[MUL1]);

	/* params:       time, curr, timeout, line */
	update_and_check(    0, DRV1,    1400, __LINE__);
	update_and_check( 1400, MUL1,    3200, __LINE__);
	update_and_check( 3200, APP1,    4400, __LINE__);
	update_and_check( 4400, BCK1,    5200, __LINE__);
	update_and_check( 5200, DRV1,    6400, __LINE__);
	update_and_check( 6400, MUL1,    8200, __LINE__);
	update_and_check( 8200, APP1,    9800, __LINE__);
	update_and_check( 9800, BCK1,   10400, __LINE__);
	update_and_check(10400, DRV1,   11600, __LINE__);
	update_and_check(11600, MUL1,   14000, __LINE__);
	update_and_check(14000, APP1,   15200, __LINE__);
	update_and_check(15200, BCK1,   15800, __LINE__);
}


void Scheduler_test::Main::test_io_signal()
{
	scheduler.ready(contexts[BCK1]);
	scheduler.ready(contexts[BCK2]);
	scheduler.ready(contexts[BCK3]);
	scheduler.ready(contexts[APP1]);

	/* params:       time, curr, timeout, line */
	update_and_check(   0, APP1,    1200, __LINE__);
	update_and_check(1200, BCK1,    1700, __LINE__);
	update_and_check(1700, BCK2,    2200, __LINE__);
	update_and_check(2200, APP1,    4200, __LINE__);
	scheduler.ready(contexts[DRV1]); /* irq occurred */
	update_and_check(3000, DRV1,    4600, __LINE__);
	timer.set_time(3200);
	scheduler.ready(contexts[MUL1]); /* signal occurred */
	scheduler.unready(contexts[DRV1]);
	update_and_check(3200, MUL1,    5000, __LINE__);
	timer.set_time(3500);
	scheduler.ready(contexts[APP2]); /* signal occurred */
	scheduler.unready(contexts[MUL1]);
	update_and_check(3500, APP2,    4000, __LINE__);
	timer.set_time(3600);
	scheduler.unready(contexts[APP2]);
	update_and_check(3600, APP1,    4700, __LINE__);
	update_and_check(4700, BCK3,    5700, __LINE__);
}


void Scheduler_test::Main::test_all_and_yield()
{
	scheduler.ready(contexts[BCK1]);
	scheduler.ready(contexts[BCK2]);
	scheduler.ready(contexts[BCK3]);
	scheduler.ready(contexts[APP1]);
	scheduler.ready(contexts[APP2]);
	scheduler.ready(contexts[APP3]);
	scheduler.ready(contexts[MUL1]);
	scheduler.ready(contexts[MUL2]);
	scheduler.ready(contexts[MUL3]);
	scheduler.ready(contexts[DRV1]);
	scheduler.ready(contexts[DRV2]);
	scheduler.ready(contexts[DRV3]);

	/* params:       time, curr, timeout, line */
	update_and_check(   0, DRV1,     500, __LINE__);
	update_and_check( 500, MUL1,    1000, __LINE__);
	update_and_check(1000, DRV2,    1500, __LINE__);
	update_and_check(1500, APP1,    2000, __LINE__);
	update_and_check(2000, MUL2,    2500, __LINE__);
	update_and_check(2500, BCK1,    3000, __LINE__);
	update_and_check(3000, DRV3,    4000, __LINE__);
	update_and_check(4000, MUL3,    5000, __LINE__);
	update_and_check(5000, APP2,    5500, __LINE__);
	update_and_check(5500, APP3,    6500, __LINE__);
	update_and_check(6500, MUL2,    7000, __LINE__);
	update_and_check(7000, BCK2,    7500, __LINE__);
	timer.set_time(7100);
	scheduler.yield();
	update_and_check(7100, BCK3,    7600, __LINE__);
	timer.set_time(7200);
	scheduler.yield();
	update_and_check(7200, DRV2,    7700, __LINE__);
	update_and_check(7700, MUL1,    8700, __LINE__);
	update_and_check(8700, BCK1,    9300, __LINE__);
	update_and_check(9300, DRV1,   10300, __LINE__);
	timer.set_time(10000);
	scheduler.yield();
	update_and_check(10000, APP2,  10500, __LINE__);
	update_and_check(10500, MUL2,  11000, __LINE__);
}

void Component::construct(Env &env)
{
	{
		Scheduler_test::Main main { env };
		main.test_background_idle();
	}

	{
		Scheduler_test::Main main { env };
		main.test_one_per_group();
	}

	{
		Scheduler_test::Main main { env };
		main.test_io_signal();
	}

	{
		Scheduler_test::Main main { env };
		main.test_all_and_yield();
	}


	env.parent().exit(0);
}
