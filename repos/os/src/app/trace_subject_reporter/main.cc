/*
 * \brief  Report information about present trace subjects
 * \author Norman Feske
 * \date   2015-06-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <util/retry.h>


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
			unsigned long long recent_execution_time = 0;

			Entry(Genode::Trace::Subject_id id) : id(id) { }

			void update(Genode::Trace::Subject_info const &new_info)
			{
				unsigned long long const last_execution_time = info.execution_time().thread_context;
				info = new_info;
				recent_execution_time = info.execution_time().thread_context - last_execution_time;
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

		enum { MAX_SUBJECTS = 512 };
		Genode::Trace::Subject_id _subjects[MAX_SUBJECTS];

		void _sort_by_recent_execution_time()
		{
			Genode::List<Entry> sorted;

			while (_entries.first()) {

				/* find entry with lowest recent execution time */
				Entry *lowest = _entries.first();
				for (Entry *e = _entries.first(); e; e = e->next()) {
					if (e->recent_execution_time < lowest->recent_execution_time)
						lowest = e;
				}

				_entries.remove(lowest);
				sorted.insert(lowest);
			}

			_entries = sorted;
		}

		unsigned update_subjects(Genode::Trace::Connection &trace)
		{
			return Genode::retry<Genode::Out_of_ram>(
				[&] () { return trace.subjects(_subjects, MAX_SUBJECTS); },
				[&] () { trace.upgrade_ram(4096); }
			);
		}

	public:

		void update(Genode::Trace::Connection &trace, Genode::Allocator &alloc)
		{
			unsigned const num_subjects = update_subjects(trace);

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

			_sort_by_recent_execution_time();
		}

		void report(Genode::Xml_generator &xml,
		            bool report_affinity, bool report_activity)
		{
			for (Entry const *e = _entries.first(); e; e = e->next()) {
				xml.node("subject", [&] () {
					xml.attribute("label", e->info.session_label().string());
					xml.attribute("thread", e->info.thread_name().string());
					xml.attribute("id", e->id.id);

					typedef Genode::Trace::Subject_info Subject_info;
					Subject_info::State const state = e->info.state();
					xml.attribute("state", Subject_info::state_name(state));

					if (report_activity)
						xml.node("activity", [&] () {
							xml.attribute("total", e->info.execution_time().thread_context);
							xml.attribute("recent", e->recent_execution_time);
						});

					if (report_affinity)
						xml.node("affinity", [&] () {
							xml.attribute("xpos", e->info.affinity().xpos());
							xml.attribute("ypos", e->info.affinity().ypos());
						});
				});
			}
		}
};


namespace App {

	struct Main;
	using namespace Genode;
}


struct App::Main
{
	Env &_env;

	Trace::Connection _trace { _env, 128*1024, 32*1024, 0 };

	Reporter _reporter { _env, "trace_subjects", "trace_subjects", 64*1024 };

	static uint64_t _default_period_ms() { return 5000; }

	uint64_t _period_ms = _default_period_ms();

	bool _report_affinity = false;
	bool _report_activity = false;

	Attached_rom_dataspace _config { _env, "config" };

	bool _config_report_attribute_enabled(char const *attr) const
	{
		try {
			return _config.xml().sub_node("report").attribute_value(attr, false);
		} catch (...) { return false; }
	}

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

		_reporter.enabled(true);
	}
};


void App::Main::_handle_config()
{
	_config.update();

	_period_ms = _config.xml().attribute_value("period_ms", _default_period_ms());

	_report_affinity = _config_report_attribute_enabled("affinity");
	_report_activity = _config_report_attribute_enabled("activity");

	_timer.trigger_periodic(1000*_period_ms);
}


void App::Main::_handle_period()
{
	/* update subject information */
	_trace_subject_registry.update(_trace, _heap);

	/* generate report */
	_reporter.clear();
	Genode::Reporter::Xml_generator xml(_reporter, [&] ()
	{
		_trace_subject_registry.report(xml, _report_affinity, _report_activity);
	});
}


void Component::construct(Genode::Env &env) { static App::Main main(env); }

