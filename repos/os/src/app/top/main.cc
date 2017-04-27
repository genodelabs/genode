/*
 * \brief  Application to show highest CPU consumer per CPU via LOG session
 * \author Norman Feske
 *         Alexander Boettcher
 * \date   2015-06-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>


struct Trace_subject_registry
{
	private:

		struct Entry : Genode::List<Entry>::Element
		{
			Genode::Trace::Subject_id const id;

			Genode::Trace::Subject_info info;

			/**
			 * Execution time during the last period
			 */
			unsigned long long recent_execution_time = 0;

			Entry(Genode::Trace::Subject_id id) : id(id) { }

			void update(Genode::Trace::Subject_info const &new_info)
			{
				if (new_info.execution_time().value < info.execution_time().value)
					recent_execution_time = 0;
				else
					recent_execution_time = new_info.execution_time().value -
				                            info.execution_time().value;

				info = new_info;
			}
		};

		Genode::List<Entry> _entries;

		Entry *_lookup(Genode::Trace::Subject_id const id)
		{
			for (Entry *e = _entries.first(); e; e = e->next())
				if (e->id == id)
					return e;

			return nullptr;
		}

		enum { MAX_SUBJECTS = 512 };
		Genode::Trace::Subject_id _subjects[MAX_SUBJECTS];

		enum { MAX_CPUS_X = 16, MAX_CPUS_Y = 1, MAX_ELEMENTS_PER_CPU = 6};

		/* accumulated execution time on all CPUs */
		unsigned long long total [MAX_CPUS_X][MAX_CPUS_Y];

		/* most significant consumer per CPU */
		Entry const * load[MAX_CPUS_X][MAX_CPUS_Y][MAX_ELEMENTS_PER_CPU];

	public:

		void update(Genode::Trace::Connection &trace, Genode::Allocator &alloc)
		{
			unsigned const num_subjects = trace.subjects(_subjects, MAX_SUBJECTS);

			/* add and update existing entries */
			for (unsigned i = 0; i < num_subjects; i++) {

				Genode::Trace::Subject_id const id = _subjects[i];

				Entry *e = _lookup(id);
				if (!e) {
					e = new (alloc) Entry(id);
					_entries.insert(e);
				}

				e->update(trace.subject_info(id));

				/* purge dead threads */
				if (e->info.state() == Genode::Trace::Subject_info::DEAD) {
					trace.free(e->id);
					_entries.remove(e);
					Genode::destroy(alloc, e);
				}
			}
		}

		void top()
		{
			/* clear old calculations */
			Genode::memset(total, 0, sizeof(total));
			Genode::memset(load, 0, sizeof(load));

			for (Entry const *e = _entries.first(); e; e = e->next()) {

				/* collect highest execution time per CPU */
				unsigned const x = e->info.affinity().xpos();
				unsigned const y = e->info.affinity().ypos();
				if (x >= MAX_CPUS_X || y >= MAX_CPUS_Y) {
					Genode::error("cpu ", e->info.affinity().xpos(), ".",
					              e->info.affinity().ypos(), " is outside "
					              "supported range ",
					              (int)MAX_CPUS_X, ".", (int)MAX_CPUS_Y);
					continue;
				}

				total[x][y] += e->recent_execution_time;

				enum { NONE = ~0U };
				unsigned replace = NONE;

				for (unsigned i = 0; i < MAX_ELEMENTS_PER_CPU; i++) {
					if (load[x][y][i])
						continue;

					replace = i;
					break;
				}

				if (replace != NONE) {
					load[x][y][replace] = e;
					continue;
				}

				for (unsigned i = 0; i < MAX_ELEMENTS_PER_CPU; i++) {
					if (e->recent_execution_time
					    <= load[x][y][i]->recent_execution_time)
						continue;

					if (replace == NONE) {
						replace = i;
						continue;
					}
					if (load[x][y][replace]->recent_execution_time
					    > load[x][y][i]->recent_execution_time)
						replace = i;
				}

				if (replace != NONE)
					load[x][y][replace] = e;
			}

			/* sort */
			for (unsigned x = 0; x < MAX_CPUS_X; x++) {
				for (unsigned y = 0; y < MAX_CPUS_Y; y++) {
					for (unsigned k = 0; k < MAX_ELEMENTS_PER_CPU;) {
						if (!load[x][y][k])
							break;

						unsigned i = k;
						for (unsigned j = i; j < MAX_ELEMENTS_PER_CPU; j++) {
							if (!load[x][y][j])
								break;

							if (load[x][y][i]->recent_execution_time
							    < load[x][y][j]->recent_execution_time) {

								Entry const * tmp = load[x][y][j];
								load[x][y][j] = load[x][y][i];
								load[x][y][i] = tmp;

								i++;
								if (i >= MAX_ELEMENTS_PER_CPU || !load[x][y][i])
									break;
							}
						}
						if (i == k)
							k++;
					}
				}
			}

			for (unsigned x = 0; x < MAX_CPUS_X; x++) {
				for (unsigned y = 0; y < MAX_CPUS_Y; y++) {
					for (unsigned i = 0; i < MAX_ELEMENTS_PER_CPU; i++) {
						if (!load[x][y][i] || !total[x][y])
							continue;

						unsigned percent = load[x][y][i]->recent_execution_time * 100   / total[x][y];
						unsigned rest    = load[x][y][i]->recent_execution_time * 10000 / total[x][y] - (percent * 100);

						using Genode::log;
						log("cpu=", load[x][y][i]->info.affinity().xpos(), ".",
						    load[x][y][i]->info.affinity().ypos(), " ",
						    percent < 10 ? "  " : (percent < 100 ? " " : ""),
						    percent, ".", rest < 10 ? "0" : "", rest, "% "
						    "thread='", load[x][y][i]->info.thread_name(), "' "
						    "label='", load[x][y][i]->info.session_label(), "'");
					}
				}
			}
			if (load[0][0][0] && load[0][0][0]->recent_execution_time)
				Genode::log("");
		}
};


namespace App {

	struct Main;
	using namespace Genode;
}


struct App::Main
{
	Env &_env;

	Trace::Connection _trace { _env, 512*1024, 32*1024, 0 };

	static unsigned long _default_period_ms() { return 5000; }

	unsigned long _period_ms = _default_period_ms();

	Attached_rom_dataspace _config { _env, "config" };

	Timer::Connection _timer { _env };

	Heap _heap { _env.ram(), _env.rm() };

	Trace_subject_registry _trace_subject_registry;

	void _handle_config();

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	void _handle_period();

	Signal_handler<Main> _periodic_handler = {
		_env.ep(), *this, &Main::_handle_period};

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		_timer.sigh(_periodic_handler);
	}
};


void App::Main::_handle_config()
{
	_config.update();

	_period_ms = _config.xml().attribute_value("period_ms", _default_period_ms());

	log("period_ms=", _period_ms);

	_timer.trigger_periodic(1000*_period_ms);
}


void App::Main::_handle_period()
{
	/* update subject information */
	_trace_subject_registry.update(_trace, _heap);

	/* show most significant consumers */
	_trace_subject_registry.top();
}


void Component::construct(Genode::Env &env) { static App::Main main(env); }

