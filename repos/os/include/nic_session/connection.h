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

namespace Nic { struct Connection; }


struct Nic::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Nic::Session> _session(Genode::Parent &parent,
	                                  char const *label,
	                                  Genode::size_t tx_buf_size,
	                                  Genode::size_t rx_buf_size)
	{
		return session(parent,
		               "ram_quota=%zd, tx_buf_size=%zd, rx_buf_size=%zd, label=\"%s\"",
		               6*4096 + tx_buf_size + rx_buf_size, tx_buf_size, rx_buf_size, label);
	}

	/**
	 * Constructor
	 *
	 * \param tx_buffer_alloc  allocator used for managing the
	 *                         transmission buffer
	 * \param tx_buf_size      size of transmission buffer in bytes
	 * \param rx_buf_size      size of reception buffer in bytes
	 */
	Connection(Genode::Env             &env,
	           Genode::Range_allocator *tx_block_alloc,
	           Genode::size_t           tx_buf_size,
	           Genode::size_t           rx_buf_size,
	           char const              *label = "")
	:
		Genode::Connection<Session>(env, _session(env.parent(), label,
		                                          tx_buf_size, rx_buf_size)),
		Session_client(cap(), tx_block_alloc)
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection(Genode::Range_allocator *tx_block_alloc,
	           Genode::size_t           tx_buf_size,
	           Genode::size_t           rx_buf_size,
	           char const              *label = "")
	:
		Genode::Connection<Session>(_session(*Genode::env()->parent(), label,
		                                     tx_buf_size, rx_buf_size)),
		Session_client(cap(), tx_block_alloc)
	{ }
};

#endif /* _INCLUDE__NIC_SESSION__CONNECTION_H_ */
