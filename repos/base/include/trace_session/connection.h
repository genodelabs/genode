/*
 * \brief  Connection to TRACE service
 * \author Norman Feske
 * \date   2013-08-11
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
	 * Constructor
	 *
	 * \param ram_quota        RAM donated for tracing purposes
	 * \param arg_buffer_size  session argument-buffer size
	 * \param parent_levels    number of parent levels to trace
	 */
	Connection(size_t ram_quota, size_t arg_buffer_size, unsigned parent_levels) :
		Genode::Connection<Session>(
			session("ram_quota=%zu, arg_buffer_size=%zu, parent_levels=%u",
			        ram_quota, arg_buffer_size, parent_levels)),
		Session_client(cap()) { }
};

#endif /* _INCLUDE__TRACE_SESSION__CONNECTION_H_ */
