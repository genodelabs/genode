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
#include <util/formatted_output.h>

using namespace Genode;


/*******************************
 ** Text-formatting utilities **
 *******************************/

struct Formatted_affinity
{
	Genode::Affinity::Location affinity;

	void print(Genode::Output &out) const
	{
		Genode::print(out, " at (", affinity.xpos(),",", affinity.ypos(), ")");
	}
};


template<typename T>
struct Formatted
{
	char const *type = "";

	T const &value;

	Formatted(T const &value) : value(value) { }
	Formatted(char const *type, T const &value) : type(type), value(value) { }

	void print(Genode::Output &out) const
	{
		Genode::print(out, " ", type, value);
	}
};


struct Quoted_name
{
	Genode::String<100> const name;

	void print(Genode::Output &out) const
	{
		Genode::print(out, "\"", name, "\"");
	}
};


template <typename T>
struct Conditional
{
	bool const _cond;
	T    const &_arg;

	Conditional(bool cond, T const &arg) : _cond(cond), _arg(arg) { }

	void print(Output &out) const
	{
		if (_cond)
			Genode::print(out, _arg);
	}
};


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


Monitor::Monitor(Trace::Connection      &trace,
                 Region_map             &rm,
                 Trace::Subject_id const subject_id)
:
	Monitor_base(trace, rm, subject_id),
	_subject_id(subject_id), _buffer(_buffer_raw)
{ }


void Monitor::update_info(Trace::Subject_info const &info)
{
	_recent_exec_time = {
		info.execution_time().thread_context     - _info.execution_time().thread_context,
		info.execution_time().scheduling_context - _info.execution_time().scheduling_context
	};

	_info = info;
}


void Monitor::apply_formatting(Formatting &formatting) const
{
	auto expand = [] (unsigned &n, auto const &arg)
	{
		n = max(n, printed_length(arg));
	};

	typedef Trace::Subject_info Subject_info;

	expand(formatting.thread_name, Formatted("Thread ", Quoted_name{_info.thread_name()}));
	expand(formatting.affinity,    Formatted_affinity{_info.affinity()});
	expand(formatting.state,       Formatted(Subject_info::state_name(_info.state())));
	expand(formatting.prio,        Formatted("prio:", _info.execution_time().priority));
	expand(formatting.total_tc,    Formatted("total:", _info.execution_time().thread_context));
	expand(formatting.recent_tc,   Formatted("recent:", _recent_exec_time.thread_context));
	expand(formatting.total_sc,    Formatted("total_sc:", _info.execution_time().scheduling_context));
	expand(formatting.recent_sc,   Formatted("recent_sc:", _recent_exec_time.scheduling_context));
}


void Monitor::print(Formatting fmt, Level_of_detail detail)
{
	/* skip output for a subject with no recent activity */
	if (detail.active_only && !recently_active())
		return;

	/* print general subject information */
	typedef Trace::Subject_info Subject_info;
	Subject_info::State const state = _info.state();

	log(Left_aligned(fmt.thread_name, Formatted("Thread ", Quoted_name{_info.thread_name()})),
	    Left_aligned(fmt.affinity, Formatted_affinity{_info.affinity()}),
	    " ",
	    Conditional(detail.state, Left_aligned(fmt.state, Formatted("", Subject_info::state_name(state)))),
	    Conditional(detail.prio, Left_aligned(fmt.prio, Formatted("prio:", _info.execution_time().priority))),
	    Left_aligned(fmt.total_tc,  Formatted("total:",  _info.execution_time().thread_context)),
	    Left_aligned(fmt.recent_tc, Formatted("recent:", _recent_exec_time.thread_context)),
	    Conditional(detail.sc_time, Left_aligned(fmt.total_sc,  Formatted("total_sc:",  _info.execution_time().scheduling_context))),
	    Conditional(detail.sc_time, Left_aligned(fmt.recent_sc, Formatted("recent_sc:", _recent_exec_time.scheduling_context))));

	/* print all buffer entries that we haven't yet printed */
	_buffer.for_each_new_entry([&] (Trace::Buffer::Entry entry) {

		/* get readable data length and skip empty entries */
		size_t length = min(entry.length(), (unsigned)MAX_ENTRY_LENGTH - 1);
		if (!length)
			return true;

		/* copy entry data from buffer and add terminating '0' */
		memcpy(_curr_entry_data, entry.data(), length);
		_curr_entry_data[length] = '\0';

		/* avoid output of empty lines due to end of line character at end */
		if (_curr_entry_data[length - 1] == '\n')
			_curr_entry_data[length - 1] = '\0';

		log("  ", Cstring(_curr_entry_data));

		return true;
	});
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
