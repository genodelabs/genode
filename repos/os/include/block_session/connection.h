/*
 * \brief  Connection to block service
 * \author Stefan Kalkowski
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK_SESSION__CONNECTION_H_
#define _INCLUDE__BLOCK_SESSION__CONNECTION_H_

#include <block_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Block { struct Connection; }

struct Block::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Constructor
	 *
	 * \param tx_buffer_alloc  allocator used for managing the
	 *                         transmission buffer
	 * \param tx_buf_size      size of transmission buffer in bytes
	 */
	Connection(Genode::Env             &env,
	           Genode::Range_allocator *tx_block_alloc,
	           Genode::size_t           tx_buf_size = 128*1024,
	           const char              *label = "")
	:
		Genode::Connection<Session>(env,
			session(env.parent(),
			        "ram_quota=%ld, cap_quota=%ld, tx_buf_size=%ld, label=\"%s\"",
			        14*1024 + tx_buf_size, CAP_QUOTA, tx_buf_size, label)),
		Session_client(cap(), *tx_block_alloc, env.rm())
	{ }
};

#endif /* _INCLUDE__BLOCK_SESSION__CONNECTION_H_ */
