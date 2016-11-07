/*
 * \brief  Connection to I/O-memory service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IO_MEM_SESSION__CONNECTION_H_
#define _INCLUDE__IO_MEM_SESSION__CONNECTION_H_

#include <io_mem_session/client.h>
#include <base/connection.h>

namespace Genode { struct Io_mem_connection; }


struct Genode::Io_mem_connection : Connection<Io_mem_session>, Io_mem_session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Io_mem_session> _session(Parent &parent, addr_t base, size_t size,
	                                    bool write_combined)
	{
		return session("ram_quota=4K, base=0x%p, size=0x%lx, wc=%s",
		               base, size, write_combined ? "yes" : "no");
	}

	/**
	 * Constructor
	 *
	 * \param base            physical base address of memory-mapped I/O resource
	 * \param size            size memory-mapped I/O resource
	 * \param write_combined  enable write-combined access to I/O memory
	 */
	Io_mem_connection(Env &env, addr_t base, size_t size, bool write_combined = false)
	:
		Connection<Io_mem_session>(env, _session(env.parent(), base, size, write_combined)),
		Io_mem_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Io_mem_connection(addr_t base, size_t size, bool write_combined = false)
	:
		Connection<Io_mem_session>(_session(*env()->parent(), base, size, write_combined)),
		Io_mem_session_client(cap())
	{ }
};

#endif /* _INCLUDE__IO_MEM_SESSION__CONNECTION_H_ */
