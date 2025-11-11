/*
 * \brief  Log service used for capturing log messages of the tests
 * \author Norman Feske
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LOG_SESSION_H_
#define _LOG_SESSION_H_

#include <log_session/log_session.h>
#include <root/component.h>
#include <base/session_object.h>

#include <types.h>
#include <test.h>

namespace Depot_autopilot {
	struct Log_session;
	struct Log_root;
}


struct Depot_autopilot::Log_session : Session_object<Genode::Log_session>
{
	struct Action : Interface
	{
		virtual Test::Name curr_test_name() = 0;
		virtual void handle_log_message(Session_label const &, Span const &) = 0;
	};

	struct Version { unsigned value; };

	Version const &_curr_version, _version { _curr_version.value };

	Action &_action;

	Log_session(Entrypoint &ep, Resources const &resources, Diag diag,
	            Label const &label, Version const &version, Action &action)
	:
		Session_object<Genode::Log_session>(ep, resources, label, diag),
		_curr_version(version), _action(action)
	{ }

	void write(String const &line) override
	{
		/* ignore log messages that occurr when winding down a test */
		if (_version.value != _curr_version.value)
			return;

		char const * const start = line.string();
		size_t num_bytes = strlen(start);

		/* strip off trailing newline */
		if (num_bytes && start[num_bytes - 1] == '\n') num_bytes--;

		_action.handle_log_message(_label, { start, num_bytes });
	}
};


struct Depot_autopilot::Log_root : Root_component<Log_session>
{
	Entrypoint &_ep;

	Log_session::Action &_action;

	Session_label const _runtime_prefix;

	Log_session::Version _version { };

	Log_root(Entrypoint &ep, Allocator &md_alloc, Log_session::Action &action,
	         Session_label const &runtime_prefix)
	:
		Root_component(ep, md_alloc),
		_ep(ep), _action(action), _runtime_prefix(runtime_prefix)
	{ }

	void current_session_done() { _version.value++; }

	Create_result _create_session(const char *args, Affinity const &) override
	{
		Session_label const label { label_from_args(args) };

		auto with_skipped = [] (Span const &bytes, size_t n, auto const &fn)
		{
			n = min(n, bytes.num_bytes);
			return fn(Span { bytes.start + n, bytes.num_bytes - n });
		};

		/*
		 * Strip the runtime's identity from the label.
		 *
		 * XXX The test's identity is also rewritten to 'init' for legacy
		 *     reasons. This policy should better be removed. But this requires
		 *     the adjustment of all tests. 'Action::curr_test_name()' could
		 *     then be removed.
		 */

		Session_label const test_prefix = prefixed_label(_runtime_prefix,
		                                                 _action.curr_test_name());

		return label.with_span([&] (Span const &label) -> Create_result {
			return test_prefix.with_span([&] (Span const &prefix) -> Create_result {

				if (!label.starts_with(prefix)) {
					warning("LOG session label does not have children label-prefix");
					return Session_error::DENIED;
				}

				return with_skipped(label, prefix.num_bytes,
					[&] (Span const &tail) -> Create_result {
						return _alloc_obj(_ep,
						                  session_resources_from_args(args),
						                  session_diag_from_args(args),
						                  Session_label("init", Cstring(tail.start, tail.num_bytes)),
						                  _version, _action); });
			});
		});
	}
};

#endif /* _LOG_SESSION_H_ */
