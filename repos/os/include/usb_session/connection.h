/*
 * \brief  Client connection to USB server
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__CONNECTION_H_
#define _INCLUDE__USB_SESSION__CONNECTION_H_

#include <usb_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Usb { struct Connection; }


struct Usb::Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Env                       &env,
	           Genode::Range_allocator          *tx_block_alloc,
	           Label                      const &label = Label(),
	           Genode::size_t                    tx_buf_size = 512*1024,
	           Genode::Signal_context_capability sigh_state_changed =
	                                     Genode::Signal_context_capability())
	:
		Genode::Connection<Session>(env, label, Ram_quota { 5*4096 + tx_buf_size },
		                            Args("tx_buf_size=", tx_buf_size)),
		Session_client(cap(), *tx_block_alloc, env.rm(), sigh_state_changed)
	{ }
};

#endif /* _INCLUDE__USB_SESSION__CONNECTION_H_ */
