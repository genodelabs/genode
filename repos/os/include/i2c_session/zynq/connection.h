/*
 * \brief  Connection to i2c service
 * \author Mark Albers
 * \author Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * \date   2012-09-18
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__I2C_SESSION__CONNECTION_H_
#define _INCLUDE__I2C_SESSION__CONNECTION_H_

#include <i2c_session/zynq/client.h>
#include <base/connection.h>

namespace I2C {

	class Connection : public Genode::Connection<Session>,
	                   public Session_client
	{
	public:
		Connection(unsigned int bus_num) :
			Genode::Connection<Session>(session("ram_quota=4K, bus=%zd", bus_num)),
			Session_client(cap()) { }
	};
}

#endif /* _INCLUDE__I2C_SESSION__CONNECTION_H_ */
