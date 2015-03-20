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
		 * Because the argument string is used with the parent interface,
		 * the message-buffer size of the parent-interface provides a
		 * realistic upper bound for dimensioning the format- string
		 * buffer.
		 */
		enum { FORMAT_STRING_SIZE = Parent::Session_args::MAX_SIZE };

		Capability<SESSION_TYPE> _cap;

		On_destruction _on_destruction;

		Capability<SESSION_TYPE> _session(Affinity const &affinity,
		                                  const char *format_args, va_list list)
		{
			char buf[FORMAT_STRING_SIZE];

			String_console sc(buf, FORMAT_STRING_SIZE);
			sc.vprintf(format_args, list);

			va_end(list);

			/* call parent interface with the resulting argument buffer */
			return env()->parent()->session<SESSION_TYPE>(buf, affinity);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param cap  session capability
		 * \param od   session policy applied when destructing the connection
		 *
		 * The 'op' argument defines whether the session should automatically
		 * be closed by the destructor of the connection (CLOSE), or the
		 * session should stay open (KEEP_OPEN). The latter is useful in
		 * situations where the creator a connection merely passes the
		 * session capability of the connection to another party but never
		 * invokes any of the session's RPC functions.
		 */
		Connection(Capability<SESSION_TYPE> cap, On_destruction od = CLOSE):
			_cap(cap), _on_destruction(od) { }

		/**
		 * Destructor
		 */
		~Connection()
		{
			if (_on_destruction == CLOSE)
				env()->parent()->close(_cap);
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
		 * Shortcut for env()->parent()->session()
		 */
		Capability<SESSION_TYPE> session(const char *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			return _session(Affinity(), format_args, list);
		}

		/**
		 * Shortcut for env()->parent()->session()
		 */
		Capability<SESSION_TYPE> session(Affinity const &affinity,
		                                 char     const *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			return _session(affinity, format_args, list);
		}
};

#endif /* _INCLUDE__BASE__CONNECTION_H_ */
