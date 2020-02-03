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

enum SORT_TIME { EC_TIME = 0, SC_TIME = 1};

struct Trace_subject_registry
{
	private:

		struct Entry : Genode::List<Entry>::Element
		{
			Genode::Trace::Subject_id const id;

			Genode::Trace::Subject_info info { };

			/**
			 * Execution time during the last period
			 */
			Genode::uint64_t recent_time[2] = { 0, 0 };

			Entry(Genode::Trace::Subject_id id) : id(id) { }

			void update(Genode::Trace::Subject_info const &new_info)
			{
				if (new_info.execution_time().thread_context < info.execution_time().thread_context)
					recent_time[EC_TIME] = 0;
				else
					recent_time[EC_TIME] = new_info.execution_time().thread_context -
				                           info.execution_time().thread_context;

				if (new_info.execution_time().scheduling_context < info.execution_time().scheduling_context)
					recent_time[SC_TIME] = 0;
				else
					recent_time[SC_TIME] = new_info.execution_time().scheduling_context -
				                           info.execution_time().scheduling_context;

				info = new_info;
			}
		};

		Genode::List<Entry> _entries { };

		Entry *_lookup(Genode::Trace::Subject_id const id)
		{
			for (Entry *e = _entries.first(); e; e = e->next())
				if (e->id == id)
					return e;

			return nullptr;
		}

		enum { MAX_SUBJECTS = 1024 };
		Genode::Trace::Subject_id _subjects[MAX_SUBJECTS];

		enum { MAX_CPUS_X = 16, MAX_CPUS_Y = 4, MAX_ELEMENTS_PER_CPU = 6};

		/* accumulated execution time on all CPUs */
		unsigned long long total_first [MAX_CPUS_X][MAX_CPUS_Y];
		unsigned long long total_second [MAX_CPUS_X][MAX_CPUS_Y];

		/* most significant consumer per CPU */
		Entry const * load[MAX_CPUS_X][MAX_CPUS_Y][MAX_ELEMENTS_PER_CPU];

		bool _reconstruct_trace_connection = false;

		unsigned update_subjects(Genode::Pd_session &pd,
		                         Genode::Trace::Connection &trace)
		{
			Genode::Ram_quota ram_quota;

			do {
				try {
					return trace.subjects(_subjects, MAX_SUBJECTS);
				} catch (Genode::Out_of_ram) {
					trace.upgrade_ram(4096);
				}

				ram_quota = pd.avail_ram();
				_reconstruct_trace_connection = (ram_quota.value < 4 * 4096);

			} while (ram_quota.value >= 2 * 4096);

			return 0;
		}

	public:

		void update(Genode::Pd_session &pd, Genode::Trace::Connection &trace,
		            Genode::Allocator &alloc)
		{
			unsigned const num_subjects = update_subjects(pd, trace);

			if (num_subjects == MAX_SUBJECTS)
				Genode::error("Not enough memory for all threads - "
				              "calculated utilization is not sane nor "
				              "complete !", num_subjects);

			/* add and update existing entries */
			for (unsigned i = 0; i < num_subjects; i++) {

				Genode::Trace::Subject_id const id = _subjects[i];

				Entry *e = _lookup(id);
				if (!e) {
					e = new (alloc) Entry(id);
					_entries.insert(e);
				}

				e->update(trace.subject_info(id));

				/* remove dead threads which did not run in the last period */
				if (e->info.state() == Genode::Trace::Subject_info::DEAD &&
				    !e->recent_time[EC_TIME] && !e->recent_time[SC_TIME]) {

					trace.free(e->id);
					_entries.remove(e);
					Genode::destroy(alloc, e);
				}
			}

			if (_reconstruct_trace_connection)
				throw Genode::Out_of_ram();
		}

		void flush(Genode::Trace::Connection &trace, Genode::Allocator &alloc)
		{
			_reconstruct_trace_connection = false;

			while (Entry * const e = _entries.first()) {
					trace.free(e->id);
					_entries.remove(e);
					Genode::destroy(alloc, e);
			}
		}

		void top(enum SORT_TIME sorting)
		{
			/* clear old calculations */
			Genode::memset(total_first, 0, sizeof(total_first));
			Genode::memset(total_second, 0, sizeof(total_second));
			Genode::memset(load, 0, sizeof(load));

			unsigned const first  = sorting == EC_TIME ? EC_TIME : SC_TIME;
			unsigned const second = sorting == EC_TIME ? SC_TIME : EC_TIME;

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

				total_first[x][y] += e->recent_time[first];
				total_second[x][y] += e->recent_time[second];

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
					if (e->recent_time[first]
					    <= load[x][y][i]->recent_time[first])
						continue;

					if (replace == NONE) {
						replace = i;
						continue;
					}
					if (load[x][y][replace]->recent_time[first]
					    > load[x][y][i]->recent_time[first])
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

							if (load[x][y][i]->recent_time[first]
							    < load[x][y][j]->recent_time[first]) {

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
						if (!load[x][y][i] || !total_first[x][y])
							continue;

						Entry const &entry = *load[x][y][i];

						unsigned ec_percent = entry.recent_time[first] * 100   / total_first[x][y];
						unsigned ec_rest    = entry.recent_time[first] * 10000 / total_first[x][y] - (ec_percent * 100);

						unsigned sc_percent = 0;
						unsigned sc_rest    = 0;
						if (total_second[x][y]) {
							sc_percent = entry.recent_time[second] * 100   / total_second[x][y];
							sc_rest    = entry.recent_time[second] * 10000 / total_second[x][y] - (sc_percent * 100);
						}

						enum { NAME_SPACE = 24 };
						static char space[NAME_SPACE];
						Genode::memset(space, ' ', NAME_SPACE - 1);

						unsigned thread_name_len = entry.info.thread_name().length();
						if (!thread_name_len)
							space[NAME_SPACE - 1] = 0;
						else
						if (thread_name_len >= NAME_SPACE)
							space[0] = 0;
						else
							space[NAME_SPACE - thread_name_len] = 0;

						Genode::String<NAME_SPACE> space_string(space);

						using Genode::log;
						log("cpu=", entry.info.affinity().xpos(),
						    ".", entry.info.affinity().ypos(),
						    " ", _align_right<4>(entry.info.execution_time().priority),
						    " ", _align_right<6>(entry.info.execution_time().quantum),
						    " ", _align_right<4>(ec_percent),
						    ".", _align_right<3>(ec_rest, true), "%"
						    " ", _align_right<4>(sc_percent),
						    ".", _align_right<3>(sc_rest, true), "% "
						    "thread='", entry.info.thread_name(), "' ", space_string,
						    "label='", entry.info.session_label(), "'");
					}
				}
			}
			if (load[0][0][0] && load[0][0][0]->recent_time[first])
				Genode::log("");
		}

		template <int T>
		Genode::String<T> _align_right(Genode::uint64_t value, bool zero = false)
		{
			Genode::String<T> result(value);

			for (Genode::uint64_t i = 1, pow = 10; i < (T - 1); i++, pow *= 10) {
				if (value < pow)
					result = Genode::String<T>(zero ? "0" : " ", result);
			}

			return result;
		}
};


namespace App {

	struct Main;
	using namespace Genode;
}


struct App::Main
{
	Env &_env;

	enum {
		TRACE_RAM_QUOTA = 10 * 4096,
		ARG_BUFFER_RAM  = 32 * 1024,
		PARENT_LEVELS   = 0
	};

	Reconstructible<Trace::Connection> _trace { _env, TRACE_RAM_QUOTA,
	                                            ARG_BUFFER_RAM, PARENT_LEVELS };

	static uint64_t _default_period_ms() { return 5000; }

	uint64_t _period_ms = _default_period_ms();

	SORT_TIME _sort { EC_TIME };

	Attached_rom_dataspace _config { _env, "config" };

	Timer::Connection _timer { _env };

	Heap _heap { _env.ram(), _env.rm() };

	Trace_subject_registry _trace_subject_registry { };

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

	String<8> ec_sc(_config.xml().attribute_value("sort_time", String<8>("ec")));
	if (ec_sc == "ec")
		_sort = EC_TIME;
	else
		_sort = SC_TIME;

	log("sorting based on ",
	    _sort == EC_TIME ? "execution context (ec) [other option is scheduling context (sc)]"
	                     : "scheduling context (sc) [other option is execution context (ec)]");

	_timer.trigger_periodic(1000*_period_ms);
}


void App::Main::_handle_period()
{
	bool reconstruct = false;

	/* update subject information */
	try {
		_trace_subject_registry.update(_env.pd(), *_trace, _heap);
	} catch (Genode::Out_of_ram) {
		reconstruct = true;
	}

	/* show most significant consumers */
	_trace_subject_registry.top(_sort);

	/* by destructing the session we free up the allocated memory in core */
	if (reconstruct) {
		Genode::warning("re-construct trace session because of out of memory");

		_trace_subject_registry.flush(*_trace, _heap);

		_trace.destruct();
		_trace.construct(_env, TRACE_RAM_QUOTA, ARG_BUFFER_RAM, PARENT_LEVELS);
	}
}


void Component::construct(Genode::Env &env) { static App::Main main(env); }

