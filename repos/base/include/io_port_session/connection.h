/*
 * \brief  Connection to I/O-port service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IO_PORT_SESSION__CONNECTION_H_
#define _INCLUDE__IO_PORT_SESSION__CONNECTION_H_

#include <io_port_session/client.h>
#include <base/connection.h>

namespace Genode { struct Io_port_connection; }


struct Genode::Io_port_connection : Connection<Io_port_session>,
                                    Io_port_session_client
{
	/**
	 * Constructor
	 *
	 * \param base  base address of port range
	 * \param size  size of port range
	 */
	Io_port_connection(Env &env, unsigned base, unsigned size)
	:
		Connection<Io_port_session>(env, Label(), Ram_quota { RAM_QUOTA },
		                            Args("io_port_base=", base, ", "
		                                 "io_port_size=", size)),
		Io_port_session_client(cap())
	{ }
};

#endif /* _INCLUDE__IO_PORT_SESSION__CONNECTION_H_ */
