/*
 * \brief  Data from Trace session,
 *         e.g. CPU idle times && thread execution time
 * \author Alexander Boettcher
 * \date   2020-07-22
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "trace.h"

void Cpu::Trace::_read_idle_times(bool skip_max_idle)
{
	if (!_trace.constructed())
		return;

	_idle_slot = (_idle_slot + 1) % HISTORY;

	auto count = _trace->for_each_subject_info([&](Subject_id const &,
	                                               Subject_info const &info)
	{
		if (info.session_label() != "kernel" || info.thread_name() != "idle")
			return;

		if (info.affinity().xpos() < MAX_CORES && info.affinity().ypos() < MAX_THREADS)
			_idle_times[info.affinity().xpos()][info.affinity().ypos()][_idle_slot] = info.execution_time();
	});

	if (count.count == count.limit) {
		Genode::log("reconstruct trace session, subject_count=", count.count);
		_reconstruct();
	}

	if (skip_max_idle)
		return;

	for (unsigned x = 0; x < _space.width(); x++) {
		for (unsigned y = 0; y < _space.height(); y++) {
			Affinity::Location const idle_location(x,y);
			/* determine max available execution time by monitoring idle */
			auto const  time = diff_idle_times(idle_location);
			auto       &max  = _idle_max[x][y];
			if (time.thread_context > max.thread_context ||
			    time.scheduling_context > max.scheduling_context)
				max = time;
		}
	}
}

Genode::Trace::Subject_id
Cpu::Trace::lookup_missing_id(Session_label const &label,
                              Thread_name const &thread)
{
	Subject_id found_id { };

	do {
		auto count = _trace->for_each_subject_info([&](Subject_id const &id,
		                                               Subject_info const &info)
		{
			if (found_id.id)
				return;

			if (thread != info.thread_name())
				return;

			if (label != info.session_label())
				return;

			found_id = id;
		});

		if (count.count == count.limit) {
			Genode::log("reconstruct trace session, subject_count=", count.count);
			_reconstruct();
			found_id = Subject_id();
			continue;
		}

		if (!found_id.id) {
			Genode::error("trace id missing");
			break;
		}
	} while (!found_id.id);

	return found_id;
}

Genode::Session_label Cpu::Trace::lookup_my_label()
{
	Subject_id    found_id { };
	Session_label label("cpu_balancer");

	do {
		auto count = _trace->for_each_subject_info([&](Subject_id const &id,
		                                               Subject_info const &info)
		{
			if (info.thread_name() != label)
				return;

			Session_label match { info.session_label().prefix(), " -> ", label };
			if (info.session_label() != match)
				return;

			if (found_id.id)
				Genode::warning("Multiple CPU balancer are running, "
				                "can't determine myself for sure.");

			found_id = id;
			label    = info.session_label();
		});

		if (count.count == count.limit) {
			Genode::log("reconstruct trace session, subject_count=", count.count);
			found_id = Subject_id();
			_reconstruct();
			continue;
		}

		if (!found_id.id) {
			Genode::error("could not lookup my label");
			break;
		}
	} while (!found_id.id);

	if (found_id.id)
		warning("My label seems to be: '", label, "'");

	return label;
}
