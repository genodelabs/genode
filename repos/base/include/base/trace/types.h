/*
 * \brief  Basic types used by the tracing infrastructure
 * \author Norman Feske
 * \date   2013-08-12
 */

#ifndef _INCLUDE__BASE__TRACE__TYPES_H_
#define _INCLUDE__BASE__TRACE__TYPES_H_

/* Genode includes */
#include <util/string.h>

namespace Genode { namespace Trace {

	/*********************
	 ** Exception types **
	 *********************/

	struct Policy_too_large        : Exception { };
	struct Out_of_metadata         : Exception { };
	struct Nonexistent_subject     : Exception { };
	struct Already_traced          : Exception { };
	struct Source_is_dead          : Exception { };
	struct Nonexistent_policy      : Exception { };
	struct Traced_by_other_session : Exception { };
	struct Subject_not_traced      : Exception { };

	typedef String<160> Session_label;
	typedef String<64>  Thread_name;

	struct Policy_id;
	struct Subject_id;
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

	bool operator == (Policy_id const &other) { return id == other.id; }
};


/**
 * Session-local trace-subject identifier
 */
struct Genode::Trace::Subject_id
{
	unsigned id;

	Subject_id() : id(0) { }
	Subject_id(unsigned id) : id(id) { }

	bool operator == (Subject_id const &other) { return id == other.id; }
};


/**
 * Subject information
 */
class Genode::Trace::Subject_info
{
	public:

		enum State { INVALID, UNTRACED, TRACED, FOREIGN, ERROR, DEAD };

	private:

		Session_label _session_label;
		Thread_name   _thread_name;
		State         _state;
		Policy_id     _policy_id;

	public:

		Subject_info() : _state(INVALID) { }

		Subject_info(Session_label const &session_label,
		             Thread_name   const &thread_name,
		             State state, Policy_id policy_id)
		:
			_session_label(session_label), _thread_name(thread_name),
			_state(state), _policy_id(policy_id)
		{ }

		Session_label const &session_label() const { return _session_label; }
		Thread_name   const &thread_name()   const { return _thread_name; }
		State                state()         const { return _state; }
		Policy_id            policy_id()     const { return _policy_id; }
};

#endif /* _INCLUDE__BASE__TRACE__TYPES_H_ */
