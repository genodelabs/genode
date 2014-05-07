/*
 * \brief  Connection to I/O-port service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IO_PORT_SESSION__CONNECTION_H_
#define _INCLUDE__IO_PORT_SESSION__CONNECTION_H_

#include <io_port_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Io_port_connection : Connection<Io_port_session>,
	                            Io_port_session_client
	{
		/**
		 * Constructor
		 *
		 * \param base  base address of port range
		 * \param size  size of port range
		 */
		Io_port_connection(unsigned base, unsigned size)
		:
			Connection<Io_port_session>(
				session("ram_quota=4K, io_port_base=%u, io_port_size=%u",
				        base, size)),

			Io_port_session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__IO_PORT_SESSION__CONNECTION_H_ */
