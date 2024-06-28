/*
 * \brief  Frontend for controlling the TRACE session
 * \author Johannes Schlatow
 * \date   2022-05-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "monitor.h"

using namespace Genode;

Directory::Path Trace_recorder::Monitor::Trace_directory::subject_path(::Subject_info const &info)
{
	using Label_path = Path<Session_label::capacity()>;

	Label_path      label_path = path_from_label<Label_path>(info.session_label().string());
	Directory::Path subject_path(Directory::join(_path, label_path.string()));

	return subject_path;
}


Trace_recorder::Monitor::Config Trace_recorder::Monitor::Config::from_xml(Xml_node const &config)
{
	return {
		.session_ram =
			config.attribute_value("session_ram",
			                       Number_of_bytes(DEFAULT_TRACE_SESSION_RAM)),
		.session_arg_buffer =
			config.attribute_value("session_arg_buffer",
			                       Number_of_bytes(DEFAULT_TRACE_SESSION_ARG_BUFFER)),
		.default_buf_sz =
			config.attribute_value("default_buffer",
			                       Number_of_bytes(DEFAULT_BUFFER_SIZE)),
	};
}


void Trace_recorder::Monitor::Attached_buffer::process_events(Trace_directory &trace_directory)
{
	/* start iteration for every writer */
	_writers.for_each([&] (Writer_base &writer) {
		writer.start_iteration(trace_directory.root(),
		                       trace_directory.subject_path(info()),
		                       info());
	});

	/* iterate entries and pass each entry to every writer */
	_buffer.for_each_new_entry([&] (Trace::Buffer::Entry &entry) {
		if (entry.length() == 0)
			return true;

		_writers.for_each([&] (Writer_base &writer) {
			writer.process_event(entry.object<Trace_event_base>(), entry.length());
		});

		return true;
	});

	/* end iteration for every writer */
	_writers.for_each([&] (Writer_base &writer) { writer.end_iteration(); });
}


Session_policy Trace_recorder::Monitor::_session_policy(Trace::Subject_info const &info, Xml_node config)
{
	Session_label const label(info.session_label());
	Session_policy policy(label, config);

	/* must have policy attribute */
	if (!policy.has_attribute("policy"))
		 throw Session_policy::No_policy_defined();

	if (policy.has_attribute("thread"))
		if (policy.attribute_value("thread", Trace::Thread_name()) != info.thread_name())
			throw Session_policy::No_policy_defined();

	return policy;
}


void Trace_recorder::Monitor::_handle_timeout()
{
	_trace_buffers.for_each([&] (Attached_buffer &buf) {
		buf.process_events(*_trace_directory);
	});
}


void Trace_recorder::Monitor::start(Xml_node config)
{
	stop();

	/* create new trace directory */
	_trace_directory.construct(_env, _alloc, config, _rtc);

	using TM = Trace_recorder::Monitor;
	TM::Config const trace_config = TM::Config::from_xml(config);

	_trace.construct(_env, trace_config.session_ram,
	                       trace_config.session_arg_buffer);

	/* find matching subjects according to config and start tracing */
	using SC = Genode::Trace::Connection;
	SC::For_each_subject_info_result const info_result =
		_trace->for_each_subject_info([&] (Trace::Subject_id   const &id,
		                                   Trace::Subject_info const &info) {
		try {
			/* skip dead subjects */
			if (info.state() == Trace::Subject_info::DEAD)
				return;

			/* check if there is a matching policy in the XML config */
			Session_policy session_policy = _session_policy(info, config);

			if (!session_policy.has_attribute("policy"))
				return;

			Trace::Buffer_size const buffer_size {
				session_policy.attribute_value("buffer",
				                               Number_of_bytes(trace_config.default_buf_sz)) };

			Policy::Name const policy_name = session_policy.attribute_value("policy", Policy::Name());

			auto trace = [&] (Policy const &policy)
			{
				policy.id().with_result(
					[&] (Trace::Policy_id const policy_id) {
					if (_trace->trace(id, policy_id, buffer_size).failed())
						warning("failed to enable tracing for policy '", policy_name, "'");
					},
					[&] (Trace::Connection::Alloc_policy_error) {
						warning("skip tracing because of missing policy"); });
			};

			/* find and assign policy; create/insert if not present */
			bool const create =
				_policies.with_element(policy_name,
					[&] /* match */ (Policy & policy) {
						trace(policy);
						return false;
					},
					[&] /* no_match */ { return true; }
				);

			/* create policy if it did not exist */
			if (create) {
				Policy &policy = *new (_alloc) Policy(_env, *_trace, policy_name, _policies);
				trace(policy);
			}

			log("Inserting trace policy \"", policy_name, "\" into ",
				 info.session_label(), " -> ", info.thread_name());

			/* attach and remember trace buffer */
			Attached_buffer &buffer = *new (_alloc) Attached_buffer(_trace_buffers,
			                                                        _env,
			                                                        _trace->buffer(id),
			                                                        info,
			                                                        id);

			/* create and register writers at trace buffer */
			session_policy.for_each_sub_node([&] (Xml_node & node) {
				bool const present =
					_backends.with_element(node.type(),
						[&] /* match */ (Backend_base &backend) {
							backend.create_writer(_alloc,
							                      buffer.writers(),
							                      _trace_directory->root(),
							                      _trace_directory->subject_path(buffer.info()));
							return true;
						},
						[&] /* no_match */ { return false; }
					);

				if (!present)
					error("No writer available for <", node.type(), "/>.");
				else
					log("Enabled ", node.type(), " writer for ", info.session_label(),
					                             " -> ",         info.thread_name());
			});
		}
		catch (Session_policy::No_policy_defined) { return; }
	});

	if (info_result.count == info_result.limit)
		warning("number of subjects equals limit, results may be truncated");

	/* register timeout */
	unsigned period_ms { 0 };
	if (!config.has_attribute("period_ms"))
		error("missing XML attribute 'period_ms'");
	else
		period_ms = config.attribute_value("period_ms", period_ms);

	_timer.trigger_periodic(period_ms * 1000);
}


void Trace_recorder::Monitor::stop()
{
	_timer.trigger_periodic(0);

	_trace_buffers.for_each([&] (Attached_buffer &buf) {

		/* stop tracing */
		_trace->pause(buf.subject_id());

		/* read remaining events from buffers */
		buf.process_events(*_trace_directory);

		/* destroy writers */
		buf.writers().for_each([&] (Writer_base &writer) {
			destroy(_alloc, &writer); });

		/* detach buffer */
		_trace->free(buf.subject_id());

		/* destroy buffer */
		destroy(_alloc, &buf);
	});

	_trace_directory.destruct();

	_trace.destruct();
}
