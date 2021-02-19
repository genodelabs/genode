/*
 * \brief  Taking migrate decision depending on policy
 * \author Alexander Boettcher
 * \date   2020-07-16
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu_thread/client.h>

#include "session.h"
#include "trace.h"

void Cpu::Session::update_threads(Trace &trace, Session_label const &cpu_balancer)
{
	apply([&](Thread_capability const &cap,
	          Name const &name,
	          Subject_id &subject_id,
	          Cpu::Policy &policy, bool)
	{
		if (!subject_id.id || trace.subject_id_reread()) {
			Session_label const label(cpu_balancer, " -> ", _label);
			subject_id = trace.lookup_missing_id(label, name);
		}

		if (!subject_id.id) {
			Genode::error("subject id ", name, " missing - still !!!!");
			return false;
		}

		Affinity::Location const &base = _affinity.location();
		Affinity::Location current { base.xpos() + policy.location.xpos(),
		                             base.ypos() + policy.location.ypos(), 1, 1 };
		Execution_time     time    { };

		/* request execution time and current location */
		try {
			trace.retrieve(subject_id.id, [&] (Execution_time const time_current,
			                                   Affinity::Location const current_loc)
			{
				current = current_loc;
				time    = time_current;

				if (_verbose)
					log("[", _label, "] name='", name, "' at ",
					    current_loc.xpos(), "x", current_loc.ypos(),
					    " has ec/sc time ", time.thread_context, "/",
					                        time.scheduling_context,
					    " policy=", policy.string());
			});
		} catch (Genode::Trace::Nonexistent_subject) {
			/* how could that be ? */
			error("[", _label, "] name='", name, "'"
				    " subject id invalid ?? ", subject_id.id);

			subject_id = Subject_id();
		}

		/* update current location of thread if changed */
		if (policy.update(base, current, time))
			_report = true;

		Affinity::Location migrate_to = current;
		if (policy.migrate(_affinity.location(), migrate_to, &trace)) {
			if (_verbose)
				log("[", _label, "] name='", name, "' request to",
				    " migrate from ", current.xpos(), "x", current.ypos(),
				    " to most idle CPU at ",
				    migrate_to.xpos(), "x", migrate_to.ypos());

			Cpu_thread_client thread(cap);
			thread.affinity(migrate_to);
		}

		return false;
	});
}

void Cpu::Session::update_threads()
{
	apply([&](Thread_capability const &cap,
	          Name const &,
	          Subject_id &,
	          Cpu::Policy &policy, bool)
	{
		Affinity::Location const &base = _affinity.location();
		Location current = Location(base.xpos() + policy.location.xpos(),
		                            base.ypos() + policy.location.xpos(),
		                            1, 1);

		if (!cap.valid())
			return false;

		Affinity::Location migrate_to = current;
		if (!policy.migrate(_affinity.location(), migrate_to, nullptr))
			return false;

		Cpu_thread_client thread(cap);
		thread.affinity(migrate_to);

		return false;
	});
}
