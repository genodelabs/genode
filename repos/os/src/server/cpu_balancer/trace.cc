/*
 * \brief  Data from Trace session,
 *         e.g. CPU idle times && thread execution time
 * \author Alexander Boettcher
 * \date   2020-07-22
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
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

	for (unsigned x = 0; x < _space.width(); x++) {
		for (unsigned y = 0; y < _space.height(); y++) {
			Affinity::Location const idle_location(x,y);
			Subject_id         const &subject_id = _idle_id[x][y];

			if (!subject_id.id || _subject_id_reread)
				_lookup_missing_idle_id(idle_location);

			if (!subject_id.id) {
				_idle_times[x][y][_idle_slot] = Execution_time(0, 0);
				continue;
			}

			Subject_info const info = _trace->subject_info(subject_id);

			Affinity::Location location = info.affinity();

			if (location.xpos() != int(x) || location.ypos() != int(y)) {
				Subject_id const subject_id_old = subject_id;

				_lookup_missing_idle_id(idle_location);

				Genode::warning("idle location mismatch ", x, "x", y, " vs ",
				                location.xpos(), "x", location.ypos(), " subject id=",
				                subject_id_old.id, " vs ", _idle_id[x][y].id);
			}

			_idle_times[x][y][_idle_slot] = info.execution_time();

			if (skip_max_idle)
				continue;

			/* determine max available execution time by monitoring idle */
			auto const  time = diff_idle_times(idle_location);
			auto       &max  = _idle_max[x][y];
			if (time.thread_context > max.thread_context ||
			    time.scheduling_context > max.scheduling_context)
				max = time;
		}
	}
}

void Cpu::Trace::_lookup_missing_idle_id(Affinity::Location const &location)
{
	Subject_id found_id { };

	do {
		auto count = _trace->for_each_subject_info([&](Subject_id const &id,
		                                               Subject_info const &info)
		{
			if (found_id.id)
				return;

			if (info.affinity().xpos() != location.xpos() ||
			    info.affinity().ypos() != location.ypos())
				return;

			if (info.session_label() != "kernel" || info.thread_name() != "idle")
				return;

			_idle_id[location.xpos()][location.ypos()] = id;
			found_id = id;
		});


		if (count.count == count.limit) {
			Genode::log("reconstruct trace session, subject_count=", count.count);
			_reconstruct();
			found_id = Subject_id();
			continue;
		}

		if (!found_id.id) {
			Genode::error("idle trace id missing");
			break;
		}
	} while (!found_id.id);
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
