/*
 * \brief  Client-side Zynq Gpio session interface
 * \author Mark Albers
 * \date   2015-04-02
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO_SESSION_H__CLIENT_H_
#define _INCLUDE__GPIO_SESSION_H__CLIENT_H_

#include <gpio_session/zynq/capability.h>
#include <base/rpc_client.h>

namespace Gpio {

	struct Session_client : Genode::Rpc_client<Session>
	{
		explicit Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }

        bool write(Genode::uint32_t data, bool isChannel2 = false) { return call<Rpc_write>(data, isChannel2); }
        Genode::uint32_t read(bool isChannel2 = false) { return call<Rpc_read>(isChannel2); }
	};
}

#endif /* _INCLUDE__GPIO_SESSION_H__CLIENT_H_ */
