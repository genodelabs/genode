/*
 * \brief  Monitoring of a trace subject
 * \author Martin Stein
 * \date   2018-01-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <monitor.h>

/* Genode includes */
#include <trace_session/connection.h>

using namespace Genode;


/******************
 ** Monitor_base **
 ******************/

Monitor_base::Monitor_base(Trace::Connection &trace,
                           Region_map        &rm,
                           Trace::Subject_id  subject_id)
:
	_trace(trace), _rm(rm),
	_buffer_raw(*(Trace::Buffer *)rm.attach(_trace.buffer(subject_id)))
{ }


Monitor_base::~Monitor_base()
{
	_rm.detach(&_buffer_raw);
}


/*************
 ** Monitor **
 *************/

Monitor &Monitor::find_by_subject_id(Trace::Subject_id const subject_id)
{
	if (subject_id.id == _subject_id.id) {
		return *this; }

	bool const side = subject_id.id > _subject_id.id;

	Monitor *const monitor = Avl_node<Monitor>::child(side);
	if (!monitor) {
		throw Monitor_tree::No_match(); }

	return monitor->find_by_subject_id(subject_id);
}


Monitor::Monitor(Trace::Connection &trace,
                 Region_map        &rm,
                 Trace::Subject_id  subject_id)
:
	Monitor_base(trace, rm, subject_id),
	_subject_id(subject_id), _buffer(_buffer_raw)
{
	_update_info();
}


void Monitor::_update_info()
{
	try {
		Trace::Subject_info const &info =
			_trace.subject_info(_subject_id);

		unsigned long long const last_execution_time =
			_info.execution_time().value;

		_info = info;
		_recent_exec_time =
			_info.execution_time().value - last_execution_time;
	}
	catch (Trace::Nonexistent_subject) { warning("Cannot update subject info: Nonexistent_subject"); }
}


void Monitor::print(bool activity, bool affinity)
{
	_update_info();

	/* print general subject information */
	typedef Trace::Subject_info Subject_info;
	Subject_info::State const state = _info.state();
	log("<subject label=\"",  _info.session_label().string(),
	          "\" thread=\"", _info.thread_name().string(),
	          "\" id=\"",     _subject_id.id,
	          "\" state=\"",  Subject_info::state_name(state),
	          "\">");

	/* print subjects activity if desired */
	if (activity)
		log("   <activity total=\"",  _info.execution_time().value,
		              "\" recent=\"", _recent_exec_time,
		              "\">");

	/* print subjects affinity if desired */
	if (affinity)
		log("   <affinity xpos=\"", _info.affinity().xpos(),
		              "\" ypos=\"", _info.affinity().ypos(),
		              "\">");

	/* print all buffer entries that we haven't yet printed */
	bool printed_buf_entries = false;
	_buffer.for_each_new_entry([&] (Trace::Buffer::Entry entry) {

		/* get readable data length and skip empty entries */
		size_t length = min(entry.length(), (unsigned)MAX_ENTRY_LENGTH - 1);
		if (!length)
			return;

		/* copy entry data from buffer and add terminating '0' */
		memcpy(_curr_entry_data, entry.data(), length);
		_curr_entry_data[length] = '\0';

		/* print copied entry data out to log */
		if (!printed_buf_entries) {
			log("   <buffer>");
			printed_buf_entries = true;
		}
		log(Cstring(_curr_entry_data));
	});
	/* print end tags */
	if (printed_buf_entries)
		log("   </buffer>");
	else
		log("   <buffer />");
	log("</subject>");
}


/******************
 ** Monitor_tree **
 ******************/

Monitor &Monitor_tree::find_by_subject_id(Trace::Subject_id const subject_id)
{
	Monitor *const monitor = first();

	if (!monitor)
		throw No_match();

	return monitor->find_by_subject_id(subject_id);
}
