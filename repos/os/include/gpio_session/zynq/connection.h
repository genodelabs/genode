/*
 * \brief  Connection to Zynq Gpio session
 * \author Mark Albers
 * \date   2015-04-02
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO_SESSION__CONNECTION_H_
#define _INCLUDE__GPIO_SESSION__CONNECTION_H_

#include <gpio_session/zynq/client.h>
#include <base/connection.h>

namespace Gpio {

	struct Connection : Genode::Connection<Session>, Session_client
	{
        Connection(unsigned gpio_number)
        : Genode::Connection<Session>(session("ram_quota=8K, gpio=%zd", gpio_number)),
		  Session_client(cap()) { }
	};
}

#endif /* _INCLUDE__GPIO_SESSION__CONNECTION_H_ */
