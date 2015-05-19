/*
 * \brief  Connection to RAM service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RAM_SESSION__CONNECTION_H_
#define _INCLUDE__RAM_SESSION__CONNECTION_H_

#include <ram_session/client.h>
#include <base/connection.h>

namespace Genode { struct Ram_connection; }


struct Genode::Ram_connection : Connection<Ram_session>, Ram_session_client
{
	enum { RAM_QUOTA = 64*1024 };
	/**
	 * Constructor
	 *
	 * \param label  session label
	 */
	Ram_connection(const char *label = "", unsigned long phys_start = 0UL,
	               unsigned long phys_size = 0UL)
	:
		Connection<Ram_session>(
			session("ram_quota=%u, phys_start=0x%lx, phys_size=0x%lx, "
			        "label=\"%s\"", RAM_QUOTA, phys_start, phys_size, label)),

		Ram_session_client(cap())
	{ }
};

#endif /* _INCLUDE__RAM_SESSION__CONNECTION_H_ */
