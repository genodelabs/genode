/*
 * \brief  Connection to Uplink service
 * \author Martin Stein
 * \date   2020-11-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_SESSION__CONNECTION_H_
#define _UPLINK_SESSION__CONNECTION_H_

/* Genode includes */
#include <uplink_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>
#include <net/mac_address.h>

namespace Uplink { struct Connection; }


struct Uplink::Connection : Genode::Connection<Session>, Session_client
{
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
	           Net::Mac_address  const &mac_address,
	           char const              *label = "")
	:
		Genode::Connection<Session>(
			env,
			session(
				env.parent(),
				"ram_quota=%ld, cap_quota=%ld, mac_address=\"%s\", "
				"tx_buf_size=%ld, rx_buf_size=%ld, label=\"%s\"",
				32 * 1024 * sizeof(long) + tx_buf_size + rx_buf_size,
				CAP_QUOTA,
				Genode::String<18>(mac_address).string(),
				tx_buf_size,
				rx_buf_size,
				label)),

		Session_client(cap(), *tx_block_alloc, env.rm())
	{ }
};

#endif /* _UPLINK_SESSION__CONNECTION_H_ */
