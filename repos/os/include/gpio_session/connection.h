/*
 * \brief  Connection to Gpio session
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO_SESSION__CONNECTION_H_
#define _INCLUDE__GPIO_SESSION__CONNECTION_H_

#include <gpio_session/client.h>
#include <base/connection.h>

namespace Gpio { struct Connection; }

struct Gpio::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, unsigned long gpio_pin)
	:
		Genode::Connection<Session>(env, session(env.parent(),
		                                         "ram_quota=8K, gpio=%ld", gpio_pin)),
		Session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection(unsigned long gpio_pin)
	:
		Genode::Connection<Session>(session("ram_quota=8K, gpio=%zd", gpio_pin)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__GPIO_SESSION__CONNECTION_H_ */
