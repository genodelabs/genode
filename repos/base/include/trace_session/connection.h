/*
 * \brief  Connection to TRACE service
 * \author Norman Feske
 * \date   2013-08-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TRACE_SESSION__CONNECTION_H_
#define _INCLUDE__TRACE_SESSION__CONNECTION_H_

#include <trace_session/client.h>
#include <base/connection.h>

namespace Genode { namespace Trace { struct Connection; } }


struct Genode::Trace::Connection : Genode::Connection<Genode::Trace::Session>,
                                   Genode::Trace::Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Trace::Session> _session(Parent  &parent,
	                                    size_t   ram_quota,
	                                    size_t   arg_buffer_size,
	                                    unsigned parent_levels)
	{
		return session(parent,
		               "ram_quota=%lu, arg_buffer_size=%lu, parent_levels=%u",
		               ram_quota + 2048, arg_buffer_size, parent_levels);
	}

	/**
	 * Constructor
	 *
	 * \param ram_quota        RAM donated for tracing purposes
	 * \param arg_buffer_size  session argument-buffer size
	 * \param parent_levels    number of parent levels to trace
	 */
	Connection(Env &env, size_t ram_quota, size_t arg_buffer_size, unsigned parent_levels)
	:
		Genode::Connection<Session>(env, _session(env.parent(), ram_quota,
		                                          arg_buffer_size, parent_levels)),
		Session_client(env.rm(), cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection(size_t ram_quota, size_t arg_buffer_size, unsigned parent_levels) __attribute__((deprecated))
	:
		Genode::Connection<Session>(_session(*env_deprecated()->parent(),
		                                     ram_quota, arg_buffer_size,
		                                     parent_levels)),
		Session_client(*env_deprecated()->rm_session(), cap())
	{ }
};

#endif /* _INCLUDE__TRACE_SESSION__CONNECTION_H_ */
