/*
 * \brief  Log information about trace subjects
 * \author Martin Stein
 * \date   2018-01-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <policy.h>
#include <monitor.h>
#include <xml_node.h>

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/session_policy.h>
#include <timer_session/connection.h>
#include <util/construct_at.h>

using namespace Genode;
using Thread_name = String<40>;


class Main
{
	private:

		enum { MAX_SUBJECTS                  = 512 };
		enum { DEFAULT_PERIOD_SEC            = 5 };
		enum { DEFAULT_BUFFER                = 1024 * 4 };
		enum { DEFAULT_SESSION_ARG_BUFFER    = 1024 * 4 };
		enum { DEFAULT_SESSION_RAM           = 1024 * 1024 };
		enum { DEFAULT_SESSION_PARENT_LEVELS = 0 };

		Env                           &_env;
		Timer::Connection              _timer               { _env };
		Attached_rom_dataspace         _config_rom          { _env, "config" };
		Xml_node                const  _config              { _config_rom.xml() };
		Trace::Connection              _trace               { _env,
		                                                      _config.attribute_value("session_ram", Number_of_bytes(DEFAULT_SESSION_RAM)),
		                                                      _config.attribute_value("session_arg_buffer", Number_of_bytes(DEFAULT_SESSION_ARG_BUFFER)),
		                                                      _config.attribute_value("session_parent_levels", (unsigned)DEFAULT_SESSION_PARENT_LEVELS) };
		bool                    const  _affinity            { _config.attribute_value("affinity", false) };
		bool                    const  _activity            { _config.attribute_value("activity", false) };
		bool                    const  _verbose             { _config.attribute_value("verbose",  false) };
		Microseconds            const  _period_us           { read_sec_attr(_config, "period_sec", DEFAULT_PERIOD_SEC) };
		Number_of_bytes         const  _default_buf_sz      { _config.attribute_value("default_buffer", Number_of_bytes(DEFAULT_BUFFER)) };
		Timer::Periodic_timeout<Main>  _period              { _timer, *this, &Main::_handle_period, _period_us };
		Heap                           _heap                { _env.ram(), _env.rm() };
		Monitor_tree                   _monitors_0          { };
		Monitor_tree                   _monitors_1          { };
		bool                           _monitors_switch     { false };
		Policy_tree                    _policies            { };
		Policy_name                    _default_policy_name { _config.attribute_value("default_policy", Policy_name("null")) };
		Policy                         _default_policy      { _env, _trace, _default_policy_name };
		unsigned long                  _report_id           { 0 };
		unsigned long                  _num_subjects        { 0 };
		unsigned long                  _num_monitors        { 0 };
		Trace::Subject_id              _subjects[MAX_SUBJECTS];

		void _handle_period(Duration)
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

			/* update available subject IDs and iterate over them */
			try { _num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS); }
			catch (Out_of_ram ) { warning("Cannot list subjects: Out_of_ram" ); return; }
			catch (Out_of_caps) { warning("Cannot list subjects: Out_of_caps"); return; }
			for (unsigned i = 0; i < _num_subjects; i++) {

				Trace::Subject_id const id = _subjects[i];
				try {
					/* skip dead subjects */
					if (_trace.subject_info(id).state() == Trace::Subject_info::DEAD)
						continue;

					/* check if there is a matching policy in the XML config */
					Session_policy session_policy = _session_policy(id);
					try {
						/* lookup monitor by subject ID */
						Monitor &monitor = old_monitors.find_by_subject_id(id);

						/* move monitor from old to new tree */
						old_monitors.remove(&monitor);
						new_monitors.insert(&monitor);

					} catch (Monitor_tree::No_match) {

						/* create monitor for subject in the new tree */
						_new_monitor(new_monitors, id, session_policy);
					}
				}
				catch (Trace::Nonexistent_subject       ) { continue; }
				catch (Session_policy::No_policy_defined) { continue; }
			}
			/* all monitors in the old tree are deprecated, destroy them */
			while (Monitor *monitor = old_monitors.first())
				_destroy_monitor(old_monitors, *monitor);

			/* dump information of each monitor in the new tree */
			log("");
			log("--- Report ", _report_id++, " (", _num_monitors, "/", _num_subjects, " subjects) ---");
			new_monitors.for_each([&] (Monitor &monitor) {
				monitor.print(_activity, _affinity);
			});
		}

		void _destroy_monitor(Monitor_tree &monitors, Monitor &monitor)
		{
			if (_verbose)
				log("destroy monitor: subject ", monitor.subject_id().id);

			try { _trace.free(monitor.subject_id()); }
			catch (Trace::Nonexistent_subject) { }
			monitors.remove(&monitor);
			destroy(_heap, &monitor);
			_num_monitors--;
		}

		void _new_monitor(Monitor_tree      &monitors,
		                  Trace::Subject_id  id,
		                  Session_policy    &session_policy)
		{
			try {
				Number_of_bytes const buffer_sz   = session_policy.attribute_value("buffer", _default_buf_sz);
				Policy_name     const policy_name = session_policy.attribute_value("policy", _default_policy_name);
				try {
					_trace.trace(id.id, _policies.find_by_name(policy_name).id(), buffer_sz);
				} catch (Policy_tree::No_match) {
					Policy &policy = *new (_heap) Policy(_env, _trace, policy_name);
					_policies.insert(policy);
					_trace.trace(id.id, policy.id(), buffer_sz);
				}
				monitors.insert(new (_heap) Monitor(_trace, _env.rm(), id));
			}
			catch (Out_of_ram                    ) { warning("Cannot activate tracing: Out_of_ram"             ); return; }
			catch (Out_of_caps                   ) { warning("Cannot activate tracing: Out_of_caps"            ); return; }
			catch (Trace::Already_traced         ) { warning("Cannot activate tracing: Already_traced"         ); return; }
			catch (Trace::Source_is_dead         ) { warning("Cannot activate tracing: Source_is_dead"         ); return; }
			catch (Trace::Nonexistent_policy     ) { warning("Cannot activate tracing: Nonexistent_policy"     ); return; }
			catch (Trace::Traced_by_other_session) { warning("Cannot activate tracing: Traced_by_other_session"); return; }
			catch (Trace::Nonexistent_subject    ) { warning("Cannot activate tracing: Nonexistent_subject"    ); return; }
			catch (Region_map::Invalid_dataspace ) { warning("Cannot activate tracing: Loading policy failed"  ); return; }


			_num_monitors++;
			if (_verbose)
				log("new monitor: subject ", id.id);
		}

		Session_policy _session_policy(Trace::Subject_id id)
		{
			Trace::Subject_info info = _trace.subject_info(id);
			Session_label const label(info.session_label());
			Session_policy policy(label, _config);
			if (policy.has_attribute("thread"))
				if (policy.attribute_value("thread", Thread_name()) != info.thread_name())
					throw Session_policy::No_policy_defined();

			return policy;
		}

	public:

		Main(Env &env) : _env(env) { _policies.insert(_default_policy); }
};


void Component::construct(Env &env) { static Main main(env); }
