/*
 * \brief  Report information about present trace subjects
 * \author Norman Feske
 * \date   2015-06-15
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <os/reporter.h>
#include <os/server.h>
#include <os/config.h>
#include <base/env.h>


namespace Server { struct Main; }


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
				unsigned long long const last_execution_time = info.execution_time().value;
				info = new_info;
				recent_execution_time = info.execution_time().value - last_execution_time;
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
							xml.attribute("total", e->info.execution_time().value);
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


struct Server::Main
{
	Entrypoint &ep;

	Genode::Trace::Connection trace { 512*1024, 32*1024, 0 };

	Genode::Reporter reporter { "trace_subjects", "trace_subjects", 64*1024 };

	static unsigned long default_period_ms() { return 5000; }

	unsigned long period_ms = default_period_ms();

	bool report_affinity = false;
	bool report_activity = false;

	bool config_report_attribute_enabled(char const *attr) const
	{
		try {
			return Genode::config()->xml_node().sub_node("report")
			                                   .attribute_value(attr, false);
		} catch (...) { return false; }
	}

	Timer::Connection timer;

	Trace_subject_registry trace_subject_registry;

	void handle_config(unsigned);

	Signal_rpc_member<Main> config_dispatcher = {
		ep, *this, &Main::handle_config};

	void handle_period(unsigned);

	Signal_rpc_member<Main> periodic_dispatcher = {
		ep, *this, &Main::handle_period};

	Main(Entrypoint &ep) : ep(ep)
	{
		Genode::config()->sigh(config_dispatcher);
		handle_config(0);

		timer.sigh(periodic_dispatcher);

		reporter.enabled(true);
	}
};


void Server::Main::handle_config(unsigned)
{
	Genode::config()->reload();

	try {
		period_ms = default_period_ms();
		Genode::config()->xml_node().attribute("period_ms").value(&period_ms);
	} catch (...) { }

	report_affinity = config_report_attribute_enabled("affinity");
	report_activity = config_report_attribute_enabled("activity");

	log("period_ms=",       period_ms,       ", "
	    "report_activity=", report_activity, ", "
	    "report_affinity=", report_affinity);

	timer.trigger_periodic(1000*period_ms);
}


void Server::Main::handle_period(unsigned)
{
	/* update subject information */
	trace_subject_registry.update(trace, *Genode::env()->heap());

	/* generate report */
	reporter.clear();
	Genode::Reporter::Xml_generator xml(reporter, [&] ()
	{
		trace_subject_registry.report(xml, report_affinity, report_activity);
	});
}


namespace Server {

	char const *name() { return "trace_subject_reporter"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Main main(ep);
	}
}

