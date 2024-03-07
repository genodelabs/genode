/*
 * \brief  Alarm data-structure test
 * \author Norman Feske
 * \date   2024-03-06
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/xoroshiro.h>
#include <base/internal/alarm_registry.h>


namespace Test {

	using namespace Genode;

	struct Clock
	{
		unsigned _value;

		static constexpr unsigned LIMIT_LOG2 = 4,
		                          LIMIT      = 1 << LIMIT_LOG2,
		                          MASK       = LIMIT - 1;

		unsigned value() const { return _value & MASK; }

		void print(Genode::Output &out) const { Genode::print(out, _value); }
	};

	struct Alarm;
	using Alarms = Alarm_registry<Alarm, Clock>;

	struct Alarm : Alarms::Element
	{
		using Name = String<64>;

		Name const name;

		Alarm(auto &registry, Name const &name, Clock time)
		:
			Alarms::Element(registry, *this, time),
			name(name)
		{ }

		void print(Output &out) const
		{
			Genode::print(out, name);
		}
	};
}


void Component::construct(Genode::Env &env)
{
	using namespace Test;

	struct Panic { };

	Xoroshiro_128_plus random { 0 };

	Alarms alarms { };

	/*
	 * Test searching alarms defined for a circular clock, and the
	 * searching for the alarm scheduled next from a given time.
	 */
	{
		Alarm a0 { alarms, "a0", Clock { 0 } },
		      a1 { alarms, "a1", Clock { 1 } },
		      a2 { alarms, "a2", Clock { 2 } },
		      a3 { alarms, "a3", Clock { 3 } };

		log(alarms);

		{
			Alarm a4 { alarms, "a4", Clock { 4 } };
			log(alarms);

			alarms.for_each_in_range({ 1 }, { 3 }, [&] (Alarm const &alarm) {
				log("in range [1...3]: ", alarm); });

			alarms.for_each_in_range({ 3 }, { 1 }, [&] (Alarm const &alarm) {
				log("in range [3...1]: ", alarm); });

			for (unsigned i = 0; i < 6; i++)
				alarms.soonest(Clock { i }).with_result(
					[&] (Clock const &time) {
						log("soonest(", i, ") -> ", time); },
					[&] (Alarms::None) {
						log("soonest(", i, ") -> none");
					}
				);

			/* a4 removed */
		}
		log(alarms);

		/* a0...a3 removed */
	}

	auto check_no_alarms_present = [&]
	{
		alarms.soonest(Clock { }).with_result(
			[&] (Clock const &time) {
				error("soonest exepectedly returned ", time); },
			[&] (Alarms::None) {
				log("soonest expectedly returned None");
			}
		);
	};

	check_no_alarms_present();

	/*
	 * Create random alarms, in particular featuring the same time values.
	 * This stress-tests the AVL tree's ability to handle duplicated keys.
	 */
	{
		unsigned const N = 100;
		Constructible<Alarm> array[N] { };

		auto check_consistency = [&] (unsigned const expected_count)
		{
			Clock time { };
			unsigned count = 0;
			alarms.for_each_in_range({ 0 }, { Clock::MASK }, [&] (Alarm const &alarm) {
				count++;
				if (alarm.time.value() < time.value()) {
					error("alarms are unexpectedly not ordered");
					throw Panic { };
				}
				time = alarm.time;
			});

			if (count != expected_count) {
				error("foreach visited ", count, " alarms, expected ", expected_count);
				throw Panic { };
			}
		};

		/* construct alarms with random times */
		for (unsigned total = 0; total < N; ) {
			Clock const time { unsigned(random.value()) % Clock::MASK };
			array[total++].construct(alarms, Alarm::Name("a", total), time);
			check_consistency(total);
		}

		log(alarms);

		/* destruct alarms in random order */
		for (unsigned total = N; total > 0; total--) {

			check_consistency(total);

			/* pick Nth still existing element */
			unsigned const nth = (total*uint16_t(random.value())) >> 16;

			for (unsigned count = 0, i = 0; i < N; i++) {
				if (array[i].constructed()) {
					if (count == nth) {
						array[i].destruct();
						break;
					}
					count++;
				}
			}
		}

		check_no_alarms_present();
	}

	/*
	 * Test the purging of all alarms in a given time window
	 */
	{
		Heap heap { env.ram(), env.rm() };

		unsigned const N = 1000;

		/* schedule alarms for the whole time range */
		for (unsigned total = 0; total < N; total++) {
			Clock const time { unsigned(random.value()) % Clock::MASK };
			new (heap) Alarm(alarms, Alarm::Name("a", total), time);
		}

		auto histogram_of_scheduled_alarms = [&] (unsigned expected_total)
		{
			unsigned total = 0;
			for (unsigned i = 0; i < Clock::MASK; i++) {
				unsigned count = 0;
				alarms.for_each_in_range({ i }, { i }, [&] (Alarm const &) {
					count++; });
				log("time ", i, ": ", count, " alarms");
				total += count;
			}
			if (total != expected_total) {
				error("total number of ", total, " alarms, expected ", expected_total);
				throw Panic { };
			}
		};

		histogram_of_scheduled_alarms(N);

		unsigned triggered = 0;
		while (alarms.with_any_in_range({ 12 }, { 3 }, [&] (Alarm &alarm) {
			triggered++;
			destroy(heap, &alarm);
		}));

		log("after purging all alarms in time window 12...3:");
		histogram_of_scheduled_alarms(N - triggered);

		/* check absence of any alarms in purged range */
		{
			unsigned count = 0;
			alarms.for_each_in_range({ 12 }, { 3 }, [&] (Alarm const &) {
				count++; });

			if (count != 0) {
				error("range of purged alarms unexpectedly not empty");
				throw Panic { };
			}
		}

		/* clear up heap */
		while (alarms.with_any_in_range({ 0 }, { Clock::MASK }, [&] (Alarm &alarm) {
			destroy(heap, &alarm); }));
	}

	log("Test succeeded.");
}
