/*
 * \brief  Connection to NIC service
 * \author Norman Feske
 * \date   2009-11-13
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC_SESSION__CONNECTION_H_
#define _INCLUDE__NIC_SESSION__CONNECTION_H_

#include <nic_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Nic {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		/**
		 * Constructor
		 *
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 * \param tx_buf_size      size of transmission buffer in bytes
		 * \param rx_buf_size      size of reception buffer in bytes
		 */
		Connection(Genode::Range_allocator *tx_block_alloc,
		           Genode::size_t           tx_buf_size,
		           Genode::size_t           rx_buf_size)
		:
			Genode::Connection<Session>(
				session("ram_quota=%zd, tx_buf_size=%zd, rx_buf_size=%zd",
				        6*4096 + tx_buf_size + rx_buf_size,
				        tx_buf_size, rx_buf_size)),
			Session_client(cap(), tx_block_alloc)
		{ }
	};
}

#endif /* _INCLUDE__NIC_SESSION__CONNECTION_H_ */
