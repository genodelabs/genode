/*
 * \brief  Connection to RAM service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RAM_SESSION__CONNECTION_H_
#define _INCLUDE__RAM_SESSION__CONNECTION_H_

#include <ram_session/client.h>
#include <base/connection.h>

namespace Genode { struct Ram_connection; }


struct Genode::Ram_connection : Connection<Ram_session>, Ram_session_client
{
	enum { RAM_QUOTA = 4*1024*sizeof(long) };

	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Ram_session> _session(Parent &parent, char const *label,
	                                 addr_t phys_start, size_t phys_size)
	{
		return session(parent,
		               "ram_quota=%u, phys_start=0x%lx, phys_size=0x%lx, "
		               "label=\"%s\"", RAM_QUOTA, phys_start, phys_size, label);
	}

	/**
	 * Constructor
	 *
	 * \param label  session label
	 */
	Ram_connection(Env &env, const char *label = "", unsigned long phys_start = 0UL,
	               unsigned long phys_size = 0UL)
	:
		Connection<Ram_session>(env, _session(env.parent(), label, phys_start, phys_size)),
		Ram_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Ram_connection(const char *label = "", unsigned long phys_start = 0UL,
	               unsigned long phys_size = 0UL) __attribute__((deprecated))
	:
		Connection<Ram_session>(_session(*env_deprecated()->parent(), label, phys_start, phys_size)),
		Ram_session_client(cap())
	{ }
};

#endif /* _INCLUDE__RAM_SESSION__CONNECTION_H_ */
