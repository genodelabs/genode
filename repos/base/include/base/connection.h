/*
 * \brief  Connection to a service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CONNECTION_H_
#define _INCLUDE__BASE__CONNECTION_H_

#include <base/env.h>
#include <base/capability.h>

namespace Genode { template <typename> class Connection; }


/**
 * Representation of an open connection to a service
 */
template <typename SESSION_TYPE>
class Genode::Connection : public Noncopyable
{
	public:

		enum On_destruction { CLOSE = false, KEEP_OPEN = true };

	private:

		/*
		 * Buffer for storing the session arguments passed to the
		 * 'session' method that is called before the 'Connection' is
		 * constructed.
		 */
		enum { FORMAT_STRING_SIZE = Parent::Session_args::MAX_SIZE };

		char _session_args[FORMAT_STRING_SIZE];
		char _affinity_arg[sizeof(Affinity)];

		Parent &_parent;

		On_destruction _on_destruction;

		void _session(Parent &parent,
		              Affinity const &affinity,
		              const char *format_args, va_list list)
		{
			String_console sc(_session_args, FORMAT_STRING_SIZE);
			sc.vprintf(format_args, list);
			va_end(list);

			memcpy(_affinity_arg, &affinity, sizeof(Affinity));
		}

		Capability<SESSION_TYPE> _request_cap()
		{
			Affinity affinity;
			memcpy(&affinity, _affinity_arg, sizeof(Affinity));

			try {
				return env()->parent()->session<SESSION_TYPE>(_session_args, affinity); }
			catch (...) {
				error(SESSION_TYPE::service_name(), "-session creation failed "
				      "(", Cstring(_session_args), ")");
				throw;
			}
		}

		Capability<SESSION_TYPE> _cap = _request_cap();

	public:

		/**
		 * Constructor
		 *
		 * \param od   session policy applied when destructing the connection
		 *
		 * The 'op' argument defines whether the session should automatically
		 * be closed by the destructor of the connection (CLOSE), or the
		 * session should stay open (KEEP_OPEN). The latter is useful in
		 * situations where the creator a connection merely passes the
		 * session capability of the connection to another party but never
		 * invokes any of the session's RPC functions.
		 */
		Connection(Env &env, Capability<SESSION_TYPE>, On_destruction od = CLOSE)
		: _parent(env.parent()), _on_destruction(od) { }

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Connection(Capability<SESSION_TYPE>, On_destruction od = CLOSE)
		: _parent(*env()->parent()), _on_destruction(od) { }

		/**
		 * Destructor
		 */
		~Connection()
		{
			if (_on_destruction == CLOSE)
				_parent.close(_cap);
		}

		/**
		 * Return session capability
		 */
		Capability<SESSION_TYPE> cap() const { return _cap; }

		/**
		 * Define session policy
		 */
		void on_destruction(On_destruction od) { _on_destruction = od; }

		/**
		 * Issue session request to the parent
		 */
		Capability<SESSION_TYPE> session(Parent &parent, const char *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			_session(parent, Affinity(), format_args, list);
			return Capability<SESSION_TYPE>();
		}

		/**
		 * Issue session request to the parent
		 */
		Capability<SESSION_TYPE> session(Parent         &parent,
		                                 Affinity const &affinity,
		                                 char     const *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			_session(parent, affinity, format_args, list);
			return Capability<SESSION_TYPE>();
		}

		/**
		 * Shortcut for env()->parent()->session()
		 *
		 * \noapi
		 * \deprecated  to be removed along with Genode::env()
		 */
		Capability<SESSION_TYPE> session(const char *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			_session(*env()->parent(), Affinity(), format_args, list);
			return Capability<SESSION_TYPE>();
		}

		/**
		 * Shortcut for env()->parent()->session()
		 *
		 * \noapi
		 * \deprecated  to be removed along with Genode::env()
		 */
		Capability<SESSION_TYPE> session(Affinity const &affinity,
		                                 char     const *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			_session(affinity, format_args, list);
			return Capability<SESSION_TYPE>();
		}
};

#endif /* _INCLUDE__BASE__CONNECTION_H_ */
