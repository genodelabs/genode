/*
 * \brief  Log information about trace subjects
 * \author Martin Stein
 * \author Norman Feske
 * \date   2018-01-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/registry.h>
#include <os/session_policy.h>
#include <timer_session/connection.h>
#include <util/construct_at.h>
#include <util/formatted_output.h>
#include <util/xml_node.h>

/* local includes */
#include <policy.h>
#include <monitor.h>

using namespace Genode;
using Thread_name = String<40>;


class Main
{
	private:

		Env &_env;

		Attached_rom_dataspace _config_rom { _env, "config" };

		struct Config
		{
			size_t       session_ram;
			size_t       session_arg_buffer;
			unsigned     session_parent_levels;
			bool         verbose;
			bool         prio;
			bool         sc_time;
			Microseconds period_us;
			size_t       default_buf_sz;
			Policy_name  default_policy_name;

			static Config from_xml(Xml_node const &);

		} const _config { Config::from_xml(_config_rom.xml()) };

		Trace::Connection _trace { _env,
		                           _config.session_ram,
		                           _config.session_arg_buffer,
		                           _config.session_parent_levels };

		Timer::Connection _timer { _env };

		Timer::Periodic_timeout<Main> _period {
			_timer, *this, &Main::_handle_period, _config.period_us };

		Heap          _heap            { _env.ram(), _env.rm() };
		Monitor_tree  _monitors_0      { };
		Monitor_tree  _monitors_1      { };
		bool          _monitors_switch { false };
		Policy_dict   _policies        { };
		Policy        _default_policy  { _env, _trace, _policies,
		                                 _config.default_policy_name };
		unsigned long _report_id       { 0 };

		static void _print_monitors(Allocator &alloc, Monitor_tree const &,
		                            Monitor::Level_of_detail);

		void _update_monitors()
		{
			/*
			 * Update monitors
			 *
			 * Which monitors are held and how they are configured depends on:
			 *
			 *   1) Which subjects are available at the Trace session,
			 *   2) which tracing state the subjects are currently in,
			 *   3) the configuration of this component about which subjects
			 *      to monitor and how
			 *
			 * All this might have changed since the last call of this method.
			 * So, adapt the monitors and the monitor tree accordingly.
			 */

			/*
			 * Switch monitor trees so that the new tree is empty and the old
			 * tree contains all monitors.
			 */
			Monitor_tree &old_monitors = _monitors_switch ? _monitors_1 : _monitors_0;
			Monitor_tree &new_monitors = _monitors_switch ? _monitors_0 : _monitors_1;
			_monitors_switch = !_monitors_switch;

			/* call 'fn' for each trace subject of interest */
			auto for_each_captured_subject = [&] (auto const &fn)
			{
				_trace.for_each_subject_info([&] (Trace::Subject_id   const  id,
				                                  Trace::Subject_info const &info) {
					/* skip dead subjects */
					if (info.state() == Trace::Subject_info::DEAD)
						return;

					with_matching_policy(info.session_label(), _config_rom.xml(),

						[&] (Xml_node const &policy) {

							if (policy.has_attribute("thread"))
								if (policy.attribute_value("thread", Thread_name()) != info.thread_name())
									return;

							fn(id, info, policy);
						},
						[&] () { /* no policy matches */ }
					);
				});
			};

			/* create monitors for new subject IDs */
			for_each_captured_subject([&] (Trace::Subject_id   const  id,
			                               Trace::Subject_info const &,
			                               Xml_node            const &policy) {
				try {
					Monitor &monitor = old_monitors.find_by_subject_id(id);

					/* move monitor from old to new tree */
					old_monitors.remove(&monitor);
					new_monitors.insert(&monitor);
				}
				catch (Monitor_tree::No_match) {

					/* create monitor for subject in the new tree */
					_new_monitor(new_monitors, id, policy);
				}
			});

			/* all monitors in the old tree are deprecated, destroy them */
			while (Monitor *monitor = old_monitors.first())
				_destroy_monitor(old_monitors, *monitor);

			/* update monitors (with up-to-date trace state of new monitors) */
			for_each_captured_subject([&] (Trace::Subject_id   const  id,
			                               Trace::Subject_info const &info,
			                               Xml_node            const &) {
				try {
					new_monitors.find_by_subject_id(id).update_info(info); }
				catch (Monitor_tree::No_match) {
					error("unexpectedly failed to look up monitor for subject ", id.id);
				}
			});
		}

		void _destroy_monitor(Monitor_tree &monitors, Monitor &monitor)
		{
			if (_config.verbose)
				log("destroy monitor: subject ", monitor.subject_id().id);

			try { _trace.free(monitor.subject_id()); }
			catch (Trace::Nonexistent_subject) { }
			monitors.remove(&monitor);
			destroy(_heap, &monitor);
		}

		void _new_monitor(Monitor_tree            &monitors,
		                  Trace::Subject_id const  id,
		                  Xml_node          const &session_policy)
		{
			auto warn_msg = [] (auto reason) {
				warning("Cannot activate tracing: ", reason); };

			try {
				Number_of_bytes const buffer_sz =
					session_policy.attribute_value("buffer", _config.default_buf_sz);

				Policy_name const policy_name =
					session_policy.attribute_value("policy", _config.default_policy_name);

				_policies.with_element(policy_name,
					[&] (Policy const &policy) {
						_trace.trace(id.id, policy.id(), buffer_sz);
					},
					[&] /* no match */ {
						Policy &policy = *new (_heap) Policy(_env, _trace, _policies, policy_name);
						_trace.trace(id.id, policy.id(), buffer_sz);
					}
				);
				monitors.insert(new (_heap) Monitor(_trace, _env.rm(), id));
			}
			catch (Trace::Source_is_dead         ) { warn_msg("Source_is_dead"         ); return; }
			catch (Trace::Nonexistent_policy     ) { warn_msg("Nonexistent_policy"     ); return; }
			catch (Trace::Traced_by_other_session) { warn_msg("Traced_by_other_session"); return; }
			catch (Trace::Nonexistent_subject    ) { warn_msg("Nonexistent_subject"    ); return; }
			catch (Region_map::Invalid_dataspace ) { warn_msg("Loading policy failed"  ); return; }
		}

		void _handle_period(Duration)
		{
			_update_monitors();

			Monitor_tree &monitors = _monitors_switch ? _monitors_1 : _monitors_0;

			log("\nReport ", _report_id++, "\n");
			Monitor::Level_of_detail const detail { .state       =  _config.verbose,
			                                        .active_only = !_config.verbose,
			                                        .prio        =  _config.prio,
			                                        .sc_time     =  _config.sc_time };
			_print_monitors(_heap, monitors, detail);
		}

	public:

		Main(Env &env) : _env(env)
		{
			/*
			 * We skip the initial monitor update as the periodic timeout triggers
			 * the update immediately for the first time.
			 */
		}
};


Main::Config Main::Config::from_xml(Xml_node const &config)
{
	return {
		.session_ram           = config.attribute_value("session_ram",
		                                                Number_of_bytes(1024*1024)),
		.session_arg_buffer    = config.attribute_value("session_arg_buffer",
		                                                Number_of_bytes(1024*4)),
		.session_parent_levels = config.attribute_value("session_parent_levels", 0u),
		.verbose               = config.attribute_value("verbose",  false),
		.prio                  = config.attribute_value("priority", false),
		.sc_time               = config.attribute_value("sc_time",  false),
		.period_us             = Microseconds(config.attribute_value("period_sec", 5)
		                                     * 1'000'000),
		.default_buf_sz        = config.attribute_value("default_buffer",
		                                                Number_of_bytes(4*1024)),
		.default_policy_name   = config.attribute_value("default_policy",
		                                                Policy_name("null"))
	};
}


void Main::_print_monitors(Allocator &alloc, Monitor_tree const &monitors,
                           Monitor::Level_of_detail detail)
{
	struct Thread : Noncopyable, Interface
	{
		Monitor const &monitor;
		Thread(Monitor const &monitor) : monitor(monitor) { }
	};

	struct Pd : Noncopyable, Interface
	{
		Session_label const label;
		Registry<Registered<Thread> > threads { };
		Pd(Session_label const &label) : label(label) { }

		bool recently_active() const
		{
			bool result = false;
			threads.for_each([&] (Thread const &thread) {
				if (thread.monitor.recently_active())
					result = true; });
			return result;
		}
	};

	Registry<Registered<Pd> > pds { };

	auto with_pd = [&] (Session_label const &label, auto const &fn)
	{
		Pd *pd_ptr = nullptr;
		pds.for_each([&] (Pd &pd) {
			if (!pd_ptr && (pd.label == label))
				pd_ptr = &pd; });

		if (!pd_ptr)
			pd_ptr = new (alloc) Registered<Pd>(pds, label);

		fn(*pd_ptr);
	};

	monitors.for_each([&] (Monitor const &monitor) {
		with_pd(monitor.info().session_label(), [&] (Pd &pd) {
			new (alloc) Registered<Thread>(pd.threads, monitor); }); });

	/* determine formatting */
	Monitor::Formatting fmt { };
	pds.for_each([&] (Pd const &pd) {
		if (!detail.active_only || pd.recently_active())
			pd.threads.for_each([&] (Thread const &thread) {
				thread.monitor.apply_formatting(fmt); }); });

	pds.for_each([&] (Pd const &pd) {

		auto opt = [] (bool cond, unsigned value) { return cond ? value : 0; };

		unsigned const table_width  = fmt.thread_name
		                            + fmt.affinity
		                            + 1 /* additional space */
		                            + opt(detail.state,   fmt.state)
		                            + opt(detail.prio,    fmt.prio)
		                            + fmt.total_tc
		                            + fmt.recent_tc
		                            + opt(detail.sc_time, fmt.total_sc)
		                            + opt(detail.sc_time, fmt.recent_sc);
		unsigned const pd_width     = 4 + (unsigned)(pd.label.length() - 1) + 1;
		unsigned const excess_width = table_width - min(table_width, pd_width + 1);

		if (detail.active_only && !pd.recently_active())
			return;

		log("PD \"", pd.label, "\" ", Repeated(excess_width, Char('-')));
		pd.threads.for_each([&] (Thread const &thread) {
			const_cast<Monitor &>(thread.monitor).print(fmt, detail); });
		log("");
	});

	pds.for_each([&] (Registered<Pd> &pd) {
		pd.threads.for_each([&] (Registered<Thread> &thread) {
			destroy(alloc, &thread); });
		destroy(alloc, &pd);
	});
}


void Component::construct(Env &env) { static Main main(env); }
