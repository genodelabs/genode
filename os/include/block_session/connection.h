/*
 * \brief  Connection to block service
 * \author Stefan Kalkowski
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BLOCK_SESSION__CONNECTION_H_
#define _INCLUDE__BLOCK_SESSION__CONNECTION_H_

#include <block_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Block {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		/**
		 * Constructor
		 *
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 * \param tx_buf_size      size of transmission buffer in bytes
		 */
		Connection(Genode::Range_allocator *tx_block_alloc,
		           Genode::size_t           tx_buf_size = 128*1024,
		           const char              *label = "")
		:
			Genode::Connection<Session>(
				session("ram_quota=%zd, tx_buf_size=%zd, label=\"%s\"",
				        3*4096 + tx_buf_size, tx_buf_size, label)),
			Session_client(cap(), tx_block_alloc) { }
	};
}

#endif /* _INCLUDE__BLOCK_SESSION__CONNECTION_H_ */
