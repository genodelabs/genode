/**
 * \brief  Client connection to USB server
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _INCLUDE__USB_SESSION__CONNECTION_H_
#define _INCLUDE__USB_SESSION__CONNECTION_H_

#include <usb_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Usb { struct Connection; }


struct Usb::Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Range_allocator *tx_block_alloc,
	           unsigned long vendor_id, unsigned long product_id,
	           Genode::size_t tx_buf_size = 512 * 1024,
	           Genode::Signal_context_capability sigh_state_changed =
	           Genode::Signal_context_capability())
	:
		Genode::Connection<Session>(session("ram_quota=%zd, tx_buf_size=%zd, vendor=%lu, product=%lu",
		                                    3 * 4096 + tx_buf_size, tx_buf_size, vendor_id, product_id)),
		Session_client(cap(), tx_block_alloc, sigh_state_changed)
	{ }
};

#endif /* _INCLUDE__USB_SESSION__CONNECTION_H_ */
