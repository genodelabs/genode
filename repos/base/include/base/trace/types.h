/*
 * \brief  Basic types used by the tracing infrastructure
 * \author Norman Feske
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__TRACE__TYPES_H_
#define _INCLUDE__BASE__TRACE__TYPES_H_

/* Genode includes */
#include <util/string.h>
#include <base/affinity.h>
#include <base/session_label.h>

namespace Genode { namespace Trace {

	/*********************
	 ** Exception types **
	 *********************/

	struct Policy_too_large        : Exception { };
	struct Nonexistent_subject     : Exception { };
	struct Already_traced          : Exception { };
	struct Source_is_dead          : Exception { };
	struct Nonexistent_policy      : Exception { };
	struct Traced_by_other_session : Exception { };
	struct Subject_not_traced      : Exception { };

	typedef String<32>  Thread_name;

	struct Policy_id;
	struct Subject_id;
	struct Execution_time;
	struct Subject_info;
} }


/**
 * Session-local policy identifier
 */
struct Genode::Trace::Policy_id
{
	unsigned id;

	Policy_id() : id(0) { }
	Policy_id(unsigned id) : id(id) { }

	bool operator == (Policy_id const &other) const { return id == other.id; }
};


/**
 * Session-local trace-subject identifier
 */
struct Genode::Trace::Subject_id
{
	unsigned id;

	Subject_id() : id(0) { }
	Subject_id(unsigned id) : id(id) { }

	bool operator == (Subject_id const &other) const { return id == other.id; }
};


/**
 * Execution time of trace subject
 *
 * The value is kernel specific.
 */
struct Genode::Trace::Execution_time
{
	unsigned long long value;

	Execution_time() : value(0) { }
	Execution_time(unsigned long long value) : value(value) { }
};


/**
 * Subject information
 */
class Genode::Trace::Subject_info
{
	public:

		enum State { INVALID, UNTRACED, TRACED, FOREIGN, ERROR, DEAD };

		static char const *state_name(State state)
		{
			switch (state) {
			case INVALID:  return "INVALID";
			case UNTRACED: return "UNTRACED";
			case TRACED:   return "TRACED";
			case FOREIGN:  return "FOREIGN";
			case ERROR:    return "ERROR";
			case DEAD:     return "DEAD";
			}
			return "INVALID";
		}

	private:

		Session_label      _session_label  { };
		Thread_name        _thread_name    { };
		State              _state          { INVALID };
		Policy_id          _policy_id      { 0 };
		Execution_time     _execution_time { 0 };
		Affinity::Location _affinity       { };

	public:

		Subject_info() { }

		Subject_info(Session_label const &session_label,
		             Thread_name   const &thread_name,
		             State state, Policy_id policy_id,
		             Execution_time execution_time,
		             Affinity::Location affinity)
		:
			_session_label(session_label), _thread_name(thread_name),
			_state(state), _policy_id(policy_id),
			_execution_time(execution_time), _affinity(affinity)
		{ }

		Session_label const &session_label()  const { return _session_label; }
		Thread_name   const &thread_name()    const { return _thread_name; }
		State                state()          const { return _state; }
		Policy_id            policy_id()      const { return _policy_id; }
		Execution_time       execution_time() const { return _execution_time; }
		Affinity::Location   affinity()       const { return _affinity; }
};

#endif /* _INCLUDE__BASE__TRACE__TYPES_H_ */
