/*
 * \brief  Connection to RM service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__CONNECTION_H_
#define _INCLUDE__RM_SESSION__CONNECTION_H_

#include <rm_session/client.h>
#include <base/connection.h>

namespace Genode { struct Rm_connection; }


struct Genode::Rm_connection : Connection<Rm_session>, Rm_session_client
{
	enum { RAM_QUOTA = 64*1024 };

	/**
	 * Constructor
	 *
	 * \param start start of the managed VM-region
	 * \param size  size of the VM-region to manage
	 */
	Rm_connection(addr_t start = ~0UL, size_t size = 0) :
		Connection<Rm_session>(
			session("ram_quota=%u, start=0x%p, size=0x%zx",
			        RAM_QUOTA, start, size)),
		Rm_session_client(cap()) { }
};

#endif /* _INCLUDE__RM_SESSION__CONNECTION_H_ */
