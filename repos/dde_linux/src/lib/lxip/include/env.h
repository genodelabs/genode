/*
 * \brief  Proxy-ARP environment
 * \author Stefan Kalkowski
 * \date   2013-05-24
 *
 * A database containing all clients sorted by IP and MAC addresses.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__SERVER__NIC_BRIDGE__ENV_H_
#define _SRC__SERVER__NIC_BRIDGE__ENV_H_

#include <base/signal.h>

namespace Net {
	struct Env;
}

struct Net::Env
{
	static Genode::Signal_receiver* receiver();
};

#endif /* _SRC__SERVER__NIC_BRIDGE__ENV_H_ */
